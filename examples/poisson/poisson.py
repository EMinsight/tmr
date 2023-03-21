from __future__ import print_function
from mpi4py import MPI
from tmr import TMR
from tacs import TACS, elements, functions
import numpy as np
import argparse
import os
import poisson_function

glpts6 = [
    -0.9324695142031520278123016,
    -0.6612093864662645136613996,
    -0.2386191860831969086305017,
    0.2386191860831969086305017,
    0.6612093864662645136613996,
    0.9324695142031520278123016,
]
glwts6 = [
    0.1713244923791703450402961,
    0.3607615730481386075698335,
    0.4679139345726910473898703,
    0.4679139345726910473898703,
    0.3607615730481386075698335,
    0.1713244923791703450402961,
]


def get_quadrature_pts(m):
    x = np.zeros(len(glpts6) * m)
    for i, pt in enumerate(glpts6):
        x[i::6] = np.linspace(0, 1 - 1.0 / m, m) + 0.5 * (1.0 + glpts6[i]) / m
    return x


def quadrature(integrand):
    integral = 0.0
    for i, wt in enumerate(glwts6):
        integral += wt * np.sum(integrand[i::6])
    return integral


def poisson_evalf(x):
    R = 100.0
    return (3920.0 / 363) * (1.0 - (x[0] * x[0] + x[1] * x[1]) / R**2) ** 6


def get_disk_aggregate(comm, functional, rho, R, n=1000):
    """Evaluate the KS functional on a disk"""
    a0 = 3920.0 / 363.0
    if functional == "ks" or functional == "pnorm":
        # Compute the solution scaled to r/R
        x = get_quadrature_pts(n)
        phi = a0 * (
            -(1.0 / 196) * x**14
            + (1.0 / 24) * x**12
            - (3.0 / 20) * x**10
            + (5.0 / 16) * x**8
            - (5.0 / 12) * x**6
            + (3.0 / 8) * x**4
            - (1.0 / 4) * x**2
            + (363.0 / 3920)
        )

        if functional == "ks":
            ksmax = np.max(phi)
            integrand = R * x * np.exp(rho * (phi - ksmax))
            kssum = 2 * np.pi * (R / (2 * n)) * quadrature(integrand)
            return ksmax, ksmax + np.log(kssum) / rho
        elif functional == "pnorm":
            maxphi = np.max(phi)
            integrand = R * x * np.power((phi / maxphi), rho)
            psum = 2 * np.pi * (R / (2 * n)) * quadrature(integrand)
            return maxphi, maxphi * np.power(psum, 1.0 / rho)
    else:
        # Compute the solution scaled to r/R
        m = 2 * n
        x = get_quadrature_pts(n)
        theta = np.pi * get_quadrature_pts(m)

        dphi = a0 * (
            -(14.0 / 196) * x**13
            + (12.0 / 24) * x**11
            - (30.0 / 20) * x**9
            + (40.0 / 16) * x**7
            - (30.0 / 12) * x**5
            + (12.0 / 8) * x**3
            - (2.0 / 4) * x
        )

        poly = np.poly1d(
            [
                -a0 * 14 / 196,
                0,
                a0 * 12 / 24,
                0,
                -a0 * (30.0 / 20),
                0,
                a0 * (40.0 / 16),
                0,
                -a0 * (30.0 / 12),
                0,
                a0 * (12.0 / 8),
                0,
                -a0 * (2.0 / 4),
                0,
            ]
        )

        deriv = poly.deriv()
        for root in deriv.roots:
            if root.real >= 0.0 and root.real <= 1.0 and root.imag == 0.0:
                pmax = np.fabs(poly(root).real)
                break

        # Set the start/end locations
        step = len(theta) / comm.size
        start = step * comm.rank
        end = step * (comm.rank + 1)
        if comm.rank == comm.size - 1:
            end = len(theta)

        if functional == "ks_grad":
            integrand1 = np.zeros(m)
            for i, t in enumerate(theta[start:end]):
                integrand = R * x * np.exp(rho * (dphi * np.cos(t) - pmax))
                integrand1[i] = (R / (2 * n)) * quadrature(integrand)

            integrand1 = comm.allreduce(integrand1, op=MPI.SUM)
            kssum = 2 * (np.pi / (2 * m)) * quadrature(integrand1)
            return pmax, pmax + np.log(kssum) / rho
        elif functional == "pnorm_grad":
            integrand1 = np.zeros(m)
            for i, t in enumerate(theta[start:end]):
                integrand = R * x * np.power((np.fabs(dphi * np.cos(t)) / pmax), rho)
                integrand1[i] = (R / (2 * n)) * quadrature(integrand)

            integrand1 = comm.allreduce(integrand1, op=MPI.SUM)
            psum = 2 * (np.pi / (2 * m)) * quadrature(integrand1)
            return pmax, pmax * np.power(psum, 1.0 / rho)


def recon_basis(degree, x):
    """
    Get the quadratic shape functions centered about the point pt
    """

    if degree == 2:
        N = np.array([1.0, x[0], x[1], x[0] * x[0], x[0] * x[1], x[1] * x[1]])
    elif degree == 3:
        N = np.array(
            [
                1.0,
                x[0],
                x[1],
                x[0] * x[0],
                x[0] * x[1],
                x[1] * x[1],
                x[0] * x[0] * x[0],
                x[0] * x[0] * x[1],
                x[0] * x[1] * x[1],
                x[1] * x[1] * x[1],
            ]
        )
    elif degree == 4:
        N = np.array(
            [
                1.0,
                x[0],
                x[1],
                x[0] * x[0],
                x[0] * x[1],
                x[1] * x[1],
                x[0] * x[0] * x[0],
                x[0] * x[0] * x[1],
                x[0] * x[1] * x[1],
                x[1] * x[1] * x[1],
                x[0] * x[0] * x[0] * x[0],
                x[0] * x[0] * x[0] * x[1],
                x[0] * x[0] * x[1] * x[1],
                x[0] * x[1] * x[1] * x[1],
                x[1] * x[1] * x[1] * x[1],
            ]
        )
    elif degree == 5:
        N = np.array(
            [
                1.0,
                x[0],
                x[1],
                x[0] * x[0],
                x[0] * x[1],
                x[1] * x[1],
                x[0] * x[0] * x[0],
                x[0] * x[0] * x[1],
                x[0] * x[1] * x[1],
                x[1] * x[1] * x[1],
                x[0] * x[0] * x[0] * x[0],
                x[0] * x[0] * x[0] * x[1],
                x[0] * x[0] * x[1] * x[1],
                x[0] * x[1] * x[1] * x[1],
                x[1] * x[1] * x[1] * x[1],
                x[0] * x[0] * x[0] * x[0] * x[0],
                x[0] * x[0] * x[0] * x[0] * x[1],
                x[0] * x[0] * x[0] * x[1] * x[1],
                x[0] * x[0] * x[1] * x[1] * x[1],
                x[0] * x[1] * x[1] * x[1] * x[1],
                x[1] * x[1] * x[1] * x[1] * x[1],
            ]
        )

    return N


def elem_recon(degree, conn, Xpts, uvals, elem_list, max_dist=1.0):
    # Get a unique list of the nodes in the list
    var = []
    for elem in elem_list:
        var.extend(conn[elem])

    # Get a unique list of values
    var = list(set(var))

    # Set up the least-squares fit
    pt = np.average(Xpts[var, :], axis=0)[:2]
    dist = np.sqrt(np.sum((Xpts[var, :2] - pt) ** 2, axis=1))

    # Loop over the adjacent nodes and fit them
    dim = 6
    if degree == 3:
        dim = 10
    elif degree == 4:
        dim = 15
    elif degree == 5:
        dim = 21
    A = np.zeros((len(var), dim))
    b = np.zeros(len(var))

    for i, v in enumerate(var):
        w = np.exp(-2 * dist[i] / max_dist)

        # Compute the basis at the provided point
        b[i] = w * uvals[v]

        x = Xpts[v, :2]
        A[i, :] = w * recon_basis(degree, (x - pt) / max_dist)

    # Fit the basis
    vals, res, rank, s = np.linalg.lstsq(A, b, rcond=-1)

    return vals, pt


def computeRecon(degree, conn, Xpts, uvals, conn_refine, Xpts_refine):
    """
    Compute the planar reconstruction over the given mesh
    """

    # Compute the element->element connectivity
    nelems = conn.shape[0]

    # Create a node->element dictionary
    node_to_elems = {}
    for i in range(nelems):
        for node in conn[i, :]:
            if node in node_to_elems:
                node_to_elems[node][i] = True
            else:
                node_to_elems[node] = {i: True}

    # Now create an elements to elements list
    elem_to_elem = []
    for i in range(nelems):
        elems = {}
        for node in conn[i, :]:
            for elem in node_to_elems[node]:
                elems[elem] = True
        elem_to_elem.append(elems.keys())

    # Get the refined shape
    uvals_refine = np.zeros(Xpts_refine.shape[0])
    count = np.zeros(Xpts_refine.shape[0])

    # Compute the average element distance
    max_dist = 2.0 * np.average(
        np.sqrt(np.sum((Xpts[conn[:, 0]] - Xpts[conn[:, -1]]) ** 2, axis=1))
    )

    for elem in range(nelems):
        # Get the list of elements
        elem_list = elem_to_elem[elem]

        # Get the reconstructed values
        vals, pt = elem_recon(degree, conn, Xpts, uvals, elem_list, max_dist=max_dist)

        # Compute the refined contributions to each element
        for node in conn_refine[elem, :]:
            N = recon_basis(degree, (Xpts_refine[node, :2] - pt) / max_dist)
            uvals_refine[node] += np.dot(vals, N)
            count[node] += 1.0

    # Average the values
    uvals_refine /= count

    return uvals_refine


def computeReconSolution(
    comm, forest, assembler, forest_refined, assembler_refined, ans, ans_refined
):
    if comm.size != 1:
        raise RuntimeError("Only works with serial cases")

    # Distribute the vector
    ans.distributeValues()

    # Create the node vector
    Xpts = forest.getPoints()
    Xpts_refined = forest_refined.getPoints()

    conn = forest.getMeshConn()
    conn_refined = forest_refined.getMeshConn()

    # Find the min/max values
    num_dep = -np.min(conn)
    num_vars = np.max(conn) + 1
    num_dep_refined = -np.min(conn_refined)
    num_vars_refined = np.max(conn_refined) + 1

    # Adjust the indices so that they are always positive
    conn += num_dep
    conn_refined += num_dep_refined

    # Add + 2 to the degree of the interpolant
    mesh_order = forest.getMeshOrder()
    if mesh_order == 2:
        degree = 2
    else:
        degree = mesh_order + 1

    # Get the values
    var = np.arange(-num_dep, num_vars, dtype=np.intc)
    values = ans.getValues(var)
    ans_array = computeRecon(degree, conn, Xpts, values, conn_refined, Xpts_refined)

    # Set the values
    var = np.arange(-num_dep_refined, num_vars_refined, dtype=np.intc)
    ans_refined.setValues(var, ans_array, op=TACS.INSERT_VALUES)

    return


class CreateMe(TMR.QuadCreator):
    def __init__(self, bcs, topo, case="disk"):
        self.case = case
        TMR.QuadCreator.__init__(bcs)
        self.topo = topo
        return

    def createElement(self, order, quad):
        """Create the element"""
        elem = elements.PoissonQuad(order, evalf=poisson_evalf)
        return elem


def createRefined(case, forest, bcs, pttype=TMR.GAUSS_LOBATTO_POINTS):
    new_forest = forest.duplicate()
    new_forest.setMeshOrder(forest.getMeshOrder() + 1, pttype)
    creator = CreateMe(bcs, forest.getTopology(), case=case)
    return new_forest, creator.createTACS(new_forest)


def createProblem(
    case,
    forest,
    bcs,
    ordering,
    mesh_order=2,
    nlevels=2,
    pttype=TMR.GAUSS_LOBATTO_POINTS,
):
    # Create the forest
    forests = []
    assemblers = []

    # Create the trees, rebalance the elements and repartition
    forest.setMeshOrder(mesh_order, pttype)
    forests.append(forest)

    # Make the creator class
    creator = CreateMe(bcs, forest.getTopology(), case=case)
    assemblers.append(creator.createTACS(forest, ordering))

    while mesh_order > 2:
        mesh_order = mesh_order - 1
        forest = forests[-1].duplicate()
        forest.setMeshOrder(mesh_order, pttype)
        forest.balance(1)
        forests.append(forest)

        # Make the creator class
        creator = CreateMe(bcs, forest.getTopology(), case=case)
        assemblers.append(creator.createTACS(forest, ordering))

    for i in range(nlevels - 1):
        forest = forests[-1].coarsen()
        forest.setMeshOrder(2, pttype)
        forest.balance(1)
        forests.append(forest)

        # Make the creator class
        creator = CreateMe(bcs, forest.getTopology(), case=case)
        assemblers.append(creator.createTACS(forest, ordering))

    # Create the multigrid object
    mg = TMR.createMg(assemblers, forests, omega=0.5)

    return assemblers[0], mg


# Set the communicator
comm = MPI.COMM_WORLD

# Create an argument parser to read in arguments from the commnad line
p = argparse.ArgumentParser()
p.add_argument("--case", type=str, default="disk")
p.add_argument("--functional", type=str, default="ks")
p.add_argument("--steps", type=int, default=5)
p.add_argument("--htarget", type=float, default=10.0)
p.add_argument("--ordering", type=str, default="multicolor")
p.add_argument("--order", type=int, default=2)
p.add_argument("--ksweight", type=float, default=100.0)
p.add_argument("--uniform_refinement", action="store_true", default=False)
p.add_argument("--structured", action="store_true", default=False)
p.add_argument("--energy_error", action="store_true", default=False)
p.add_argument("--exact_refined_adjoint", action="store_true", default=False)
p.add_argument("--remesh_domain", action="store_true", default=False)
p.add_argument("--remesh_strategy", type=str, default="fraction")
p.add_argument("--element_count_target", type=float, default=20e3)
args = p.parse_args()

# Set the case type
case = args.case

# Set the type of ordering to use for this problem
ordering = args.ordering
ordering = ordering.lower()

# Set the value of the target length scale in the mesh
htarget = args.htarget

# Set the order
order = args.order

# Set the KS parameter
ksweight = args.ksweight

# Set the number of AMR steps to use
steps = args.steps

# Store whether to use a structured/unstructed mesh
structured = args.structured

# Set what functional to use
functional = str(args.functional)

# This flag indicates whether to solve the adjoint exactly on the
# next-finest mesh or not
exact_refined_adjoint = args.exact_refined_adjoint

# Set the count target
element_count_target = args.element_count_target

# The boundary condition object
bcs = TMR.BoundaryConditions()

# The radius of the disk
R = 100.0

# The side-length of the rectangular plate
L = 200.0

if case == "disk":
    # Note max_grad = 1.3650347513266252e+00
    table = {
        "ks": {
            10: 1.7194793227700980e00,
            100: 1.0476803898865747e00,
            1000: 1.0024552781857017e00,
        },
        "pnorm": {
            10: 2.0278875565653012e00,
            100: 1.0487286434118916e00,
            1000: 1.0024572906600107e00,
        },
        "ks_grad": {
            10: 2.0271422810814967e00,
            100: 1.4068698636894836e00,
            1000: 1.3669053566825087e00,
        },
        "pnorm_grad": {
            10: 2.9050104578537992e00,
            100: 1.4376337509101673e00,
            1000: 1.3689639709982468e00,
        },
    }

    if functional in table and int(ksweight) in table[functional]:
        exact_functional = table[functional][int(ksweight)]
    else:
        nrange = [500, 1000, 2000, 5000]
        if functional == "ks_grad" or functional == "pnorm_grad":
            if comm.rank == 0:
                print("functional: %10s  parameter: %g" % (functional, ksweight))
                print("%10s %25s %25s" % ("n", "max", "func. estimate"))
            for n in nrange:
                ks_max, approx = get_disk_aggregate(comm, functional, ksweight, R, n=n)
                if comm.rank == 0:
                    print("%10d %25.16e %25.16e" % (n, ks_max, approx))
                exact_functional = approx

    geo = TMR.LoadModel("2d-disk.stp")
    verts = geo.getVertices()
    edges = geo.getEdges()
    faces = geo.getFaces()

    # Set the names
    verts[1].setName("midpoint")
    for index in [0, 2, 3, 4]:
        verts[index].setName("clamped")
    for index in [2, 4, 6, 7]:
        edges[index].setName("clamped")

    # Set the boundary conditions
    bcs.addBoundaryCondition("clamped", [0])

if comm.rank == 0:
    print("Exact functional = %25.15e" % (exact_functional))

# Create the new mesh
mesh = TMR.Mesh(comm, geo)

# Set the meshing options
opts = TMR.MeshOptions()

# Set the mesh type
opts.mesh_type_default = TMR.UNSTRUCTURED
if structured:
    opts.mesh_type_default = TMR.STRUCTURED

opts.frontal_quality_factor = 1.25
opts.num_smoothing_steps = 50
opts.write_mesh_quality_histogram = 1
opts.triangularize_print_iter = 50000

# Create the surface mesh
mesh.mesh(htarget, opts)

# Create the corresponding mesh topology from the mesh-model
model = mesh.createModelFromMesh()
topo = TMR.Topology(comm, model)

# Create the quad forest and set the topology of the forest
depth = 0
if order == 2:
    depth = 1
forest = TMR.QuadForest(comm)
forest.setTopology(topo)
forest.setMeshOrder(order, TMR.UNIFORM_POINTS)
forest.createTrees(depth)

# Set the ordering to use
# if ordering == 'rcm':
#     ordering = TACS.PY_RCM_ORDER
# elif ordering == 'multicolor':
#     ordering = TACS.PY_MULTICOLOR_ORDER
# else:
ordering = TACS.PY_NATURAL_ORDER

# The target relative error
target_rel_err = 1e-4

# Open a log file to write
descript = "%s_order%d_poisson" % (case, order)
if args.uniform_refinement:
    descript += "_uniform"
descript += "_" + functional + "%g" % (ksweight)

# Add a description about the meshing strategy
if args.remesh_domain:
    descript += "_" + args.remesh_strategy
    if args.remesh_strategy == "fixed_mesh":
        descript += "_%g" % (args.element_count_target)

# Create the log file and write out the header
log_fp = open("results/%s.dat" % (descript), "w")
s = "Variables = iter, nelems, nnodes, fval, fcorr, abs_err, adjoint_corr, "
s += "exact, fval_error, fval_corr_error, "
s += "fval_effectivity, indicator_effectivity\n"
log_fp.write(s)

# Set the feature size object
feature_size = None

for k in range(steps):
    # Create the topology problem
    nlevs = min(4, depth + k + 1)
    assembler, mg = createProblem(
        case, forest, bcs, ordering, mesh_order=order, nlevels=nlevs
    )

    # Create the assembler object
    res = assembler.createVec()
    ans = assembler.createVec()

    use_direct_solve = False
    if use_direct_solve:
        mat = assembler.createFEMat()
        assembler.assembleJacobian(1.0, 0.0, 0.0, res, mat)

        # Create the direct solver
        pc = TACS.Pc(mat)
        pc.factor()
    else:
        # Solve the linear system
        mg.assembleJacobian(1.0, 0.0, 0.0, res)
        mg.factor()
        pc = mg
        mat = mg.getMat()

    # Create the GMRES object
    gmres = TACS.KSM(mat, pc, 100, isFlexible=1)
    gmres.setMonitor(comm, freq=10)
    gmres.setTolerances(1e-14, 1e-30)
    gmres.solve(res, ans)
    ans.scale(-1.0)

    # Set the variables
    assembler.setVariables(ans)

    flag = TACS.ToFH5.NODES | TACS.ToFH5.DISPLACEMENTS | TACS.ToFH5.STRAINS
    f5 = TACS.ToFH5(assembler, TACS.PY_POISSON_2D_ELEMENT, flag)
    f5.writeToFile("results/%s_solution%02d.f5" % (descript, k))

    # Create and compute the function
    fval = 0.0
    if functional == "ks":
        direction = [1.0 / R**2, 0.0, 0.0]
        func = functions.KSDisplacement(assembler, ksweight, direction)
        func.setKSDispType("continuous")
    elif functional == "pnorm":
        direction = [1.0 / R**2, 0.0, 0.0]
        func = functions.KSDisplacement(assembler, ksweight, direction)
        func.setKSDispType("pnorm-continuous")
    elif functional == "ks_grad":
        direction = [1.0 / R, 0.0]
        func = poisson_function.KSPoissonGrad(assembler, ksweight, direction)
        func.setGradFunctionType("continuous")
    elif functional == "pnorm_grad":
        direction = [1.0 / R, 0.0]
        func = poisson_function.KSPoissonGrad(assembler, ksweight, direction)
        func.setGradFunctionType("pnorm-continuous")
    fval = assembler.evalFunctions([func])[0]

    # Create the refined mesh
    if exact_refined_adjoint:
        forest_refined = forest.duplicate()
        assembler_refined, mg = createProblem(
            case, forest_refined, bcs, ordering, mesh_order=order + 1, nlevels=nlevs
        )
    else:
        forest_refined, assembler_refined = createRefined(case, forest, bcs)

    if args.energy_error:
        # Compute the strain energy error estimate
        fval_corr = 0.0
        adjoint_corr = 0.0
        err_est, error = TMR.strainEnergyError(
            forest, assembler, forest_refined, assembler_refined
        )

        TMR.computeReconSolution(forest, assembler, forest_refined, assembler_refined)
    else:
        if exact_refined_adjoint:
            # Compute the reconstructed solution on the refined mesh
            ans_interp = assembler_refined.createVec()

            if comm.size == 1:
                # Distribute the answer vector across all procs
                computeReconSolution(
                    comm,
                    forest,
                    assembler,
                    forest_refined,
                    assembler_refined,
                    ans,
                    ans_interp,
                )
            else:
                TMR.computeReconSolution(
                    forest,
                    assembler,
                    forest_refined,
                    assembler_refined,
                    ans,
                    ans_interp,
                )

            # Set the interpolated solution on the fine mesh
            assembler_refined.setVariables(ans_interp)

            # Assemble the Jacobian matrix on the refined mesh
            res_refined = assembler_refined.createVec()
            mg.assembleJacobian(1.0, 0.0, 0.0, res_refined)
            mg.factor()
            pc = mg
            mat = mg.getMat()

            # Compute the functional and the right-hand-side for the
            # adjoint on the refined mesh
            adjoint_rhs = assembler_refined.createVec()
            if functional == "ks":
                direction = [1.0 / R**2, 0.0, 0.0]
                func_refined = functions.KSDisplacement(
                    assembler_refined, ksweight, direction
                )
                func_refined.setKSDispType("continuous")
            elif functional == "pnorm":
                direction = [1.0 / R**2, 0.0, 0.0]
                func_refined = functions.KSDisplacement(
                    assembler_refined, ksweight, direction
                )
                func_refined.setKSDispType("pnorm-continuous")
            elif functional == "ks_grad":
                direction = [1.0 / R, 0.0]
                func_refined = poisson_function.KSPoissonGrad(
                    assembler_refined, ksweight, direction
                )
                func_refined.setGradFunctionType("continuous")
            elif functional == "pnorm_grad":
                direction = [1.0 / R, 0.0]
                func_refined = poisson_function.KSPoissonGrad(
                    assembler_refined, ksweight, direction
                )
                func_refined.setGradFunctionType("pnorm-continuous")

            # Evaluate the functional on the refined mesh
            fval_refined = assembler_refined.evalFunctions([func_refined])[0]
            assembler_refined.evalSVSens(func_refined, adjoint_rhs)

            # Create the GMRES object on the fine mesh
            gmres = TACS.KSM(mat, pc, 100, isFlexible=1)
            gmres.setMonitor(comm, freq=10)
            gmres.setTolerances(1e-14, 1e-30)

            # Solve the linear system
            adjoint_refined = assembler_refined.createVec()
            gmres.solve(adjoint_rhs, adjoint_refined)
            adjoint_refined.scale(-1.0)

            # Compute the adjoint correction on the fine mesh
            adjoint_corr = adjoint_refined.dot(res_refined)

            # Compute the reconstructed adjoint solution on the refined mesh
            adjoint = assembler.createVec()
            adjoint_interp = assembler_refined.createVec()
            TMR.computeInterpSolution(
                forest_refined,
                assembler_refined,
                forest,
                assembler,
                adjoint_refined,
                adjoint,
            )
            TMR.computeInterpSolution(
                forest,
                assembler,
                forest_refined,
                assembler_refined,
                adjoint,
                adjoint_interp,
            )
            adjoint_refined.axpy(-1.0, adjoint_interp)

            # Estimate the element-wise errors
            err_est, __, error = TMR.adjointError(
                forest,
                assembler,
                forest_refined,
                assembler_refined,
                ans_interp,
                adjoint_refined,
            )

            # Set the adjoint variables
            adjoint_refined.axpy(1.0, adjoint_interp)
            assembler_refined.setVariables(adjoint_refined)
        else:
            # Compute the adjoint on the original mesh
            res.zeroEntries()
            assembler.evalSVSens(func, res)
            adjoint = assembler.createVec()
            gmres.solve(res, adjoint)
            adjoint.scale(-1.0)

            # Compute the solution on the refined mesh
            ans_refined = assembler_refined.createVec()

            if comm.size == 1:
                computeReconSolution(
                    comm,
                    forest,
                    assembler,
                    forest_refined,
                    assembler_refined,
                    ans,
                    ans_refined,
                )
            else:
                TMR.computeReconSolution(
                    forest,
                    assembler,
                    forest_refined,
                    assembler_refined,
                    ans,
                    ans_refined,
                )

            # Apply Dirichlet boundary conditions
            assembler_refined.setVariables(ans_refined)

            # Assemble the residual on the refined mesh
            res_refined = assembler_refined.createVec()
            assembler_refined.assembleRes(res_refined)

            # Compute the functional on the refined mesh
            if functional == "ks":
                direction = [1.0 / R**2, 0.0, 0.0]
                func_refined = functions.KSDisplacement(
                    assembler_refined, ksweight, direction
                )
                func_refined.setKSDispType("continuous")
            elif functional == "pnorm":
                direction = [1.0 / R**2, 0.0, 0.0]
                func_refined = functions.KSDisplacement(
                    assembler_refined, ksweight, direction
                )
                func_refined.setKSDispType("pnorm-continuous")
            elif functional == "ks_grad":
                direction = [1.0 / R, 0.0]
                func_refined = poisson_function.KSPoissonGrad(
                    assembler_refined, ksweight, direction
                )
                func_refined.setGradFunctionType("continuous")
            elif functional == "pnorm_grad":
                direction = [1.0 / R, 0.0]
                func_refined = poisson_function.KSPoissonGrad(
                    assembler_refined, ksweight, direction
                )
                func_refined.setGradFunctionType("pnorm-continuous")

            # Evaluate the functional on the refined mesh
            fval_refined = assembler_refined.evalFunctions([func_refined])[0]

            # Approximate the difference between the refined adjoint
            # and the adjoint on the current mesh
            adjoint_refined = assembler_refined.createVec()
            if comm.size == 1:
                computeReconSolution(
                    comm,
                    forest,
                    assembler,
                    forest_refined,
                    assembler_refined,
                    adjoint,
                    adjoint_refined,
                )
            else:
                TMR.computeReconSolution(
                    forest,
                    assembler,
                    forest_refined,
                    assembler_refined,
                    adjoint,
                    adjoint_refined,
                )

            # Compute the adjoint correction on the fine mesh
            adjoint_corr = adjoint_refined.dot(res_refined)

            # Compute the diff between the interpolated and
            # reconstructed solutions
            adjoint_interp = assembler_refined.createVec()
            TMR.computeInterpSolution(
                forest_refined,
                assembler_refined,
                forest,
                assembler,
                adjoint_refined,
                adjoint,
            )
            TMR.computeInterpSolution(
                forest,
                assembler,
                forest_refined,
                assembler_refined,
                adjoint,
                adjoint_interp,
            )
            adjoint_refined.axpy(-1.0, adjoint_interp)

            # Estimate the element-wise errors
            err_est, __, error = TMR.adjointError(
                forest,
                assembler,
                forest_refined,
                assembler_refined,
                ans_refined,
                adjoint_refined,
            )

        # Compute the refined function value
        fval_corr = fval_refined + adjoint_corr

    f5_refine = TACS.ToFH5(assembler_refined, TACS.PY_POISSON_2D_ELEMENT, flag)
    f5_refine.writeToFile("results/%s_solution_refined%02d.f5" % (descript, k))

    # Compute the refinement from the error estimate
    low = -25
    high = 4
    bins_per_decade = 10
    nbins = bins_per_decade * (high - low)
    bounds = 10 ** np.linspace(high, low, nbins + 1)
    bins = np.zeros(nbins + 2, dtype=np.int)

    # Compute the mean and standard deviations of the log(error)
    ntotal = comm.allreduce(assembler.getNumElements(), op=MPI.SUM)
    mean = comm.allreduce(np.sum(np.log(error)), op=MPI.SUM)
    mean /= ntotal

    # Compute the standard deviation
    stddev = comm.allreduce(np.sum((np.log(error) - mean) ** 2), op=MPI.SUM)
    stddev = np.sqrt(stddev / (ntotal - 1))

    # Get the total number of nodes
    nnodes = comm.allreduce(assembler.getNumOwnedNodes(), op=MPI.SUM)

    # Compute the error from the exact solution
    fval_error = np.fabs(exact_functional - fval)
    fval_corr_error = np.fabs(exact_functional - fval_corr)

    fval_effectivity = (fval_corr - fval) / (exact_functional - fval)
    indicator_effectivity = err_est / np.fabs(exact_functional - fval)

    # Write the log data to a file
    s = "%6d %6d %6d %20.15e %20.15e %20.15e %20.15e "
    s += "%20.15e %20.15e %20.15e %20.15e %20.15e\n"
    log_fp.write(
        s
        % (
            k,
            ntotal,
            nnodes,
            fval,
            fval_corr,
            err_est,
            adjoint_corr,
            exact_functional,
            fval_error,
            fval_corr_error,
            fval_effectivity,
            indicator_effectivity,
        )
    )
    log_fp.flush()

    # Compute the bins
    for i in range(len(error)):
        if error[i] > bounds[0]:
            bins[0] += 1
        elif error[i] < bounds[-1]:
            bins[-1] += 1
        else:
            for j in range(len(bounds) - 1):
                if error[i] <= bounds[j] and error[i] > bounds[j + 1]:
                    bins[j + 1] += 1

    # Compute the number of bins
    bins = comm.allreduce(bins, MPI.SUM)

    # Compute the sum of the bins
    total = np.sum(bins)

    # Print out the result
    if comm.rank == 0:
        print("fval      = ", fval)
        print("fval corr = ", fval_corr)
        print("estimate  = ", err_est)
        print("mean      = ", mean)
        print("stddev    = ", stddev)

        # Set the data
        data = np.zeros((nbins, 4))
        for i in range(nbins - 1, -1, -1):
            data[i, 0] = bounds[i]
            data[i, 1] = bounds[i + 1]
            data[i, 2] = bins[i + 1]
            data[i, 3] = 100.0 * bins[i + 1] / total
        np.savetxt("results/%s_data%d.txt" % (descript, k), data)

    # Print out the error estimate
    assembler.setDesignVars(error)
    f5.writeToFile("results/error%02d.f5" % (k))

    # Perform the refinement
    if args.uniform_refinement:
        forest.refine()
        forest.balance(1)
        forest.repartition()
    elif k < steps - 1:
        if args.remesh_domain:
            # Ensure that we're using an unstructured mesh
            opts.mesh_type_default = TMR.UNSTRUCTURED

            # Find the positions of the center points of each node
            nelems = assembler.getNumElements()

            # Allocate the positions
            Xp = np.zeros((nelems, 3))
            for i in range(nelems):
                # Get the information about the given element
                elem, Xpt, vrs, dvars, ddvars = assembler.getElementData(i)

                # Get the approximate element centroid
                Xp[i, :] = np.average(Xpt.reshape((-1, 3)), axis=0)

            # Prepare to collect things to the root processor (only
            # one where it is required)
            root = 0

            # Get the element counts
            if comm.rank == root:
                size = error.shape[0]
                count = comm.gather(size, root=root)
                count = np.array(count, dtype=np.int)
                ntotal = np.sum(count)

                errors = np.zeros(np.sum(count))
                Xpt = np.zeros(3 * np.sum(count))
                comm.Gatherv(error, [errors, count])
                comm.Gatherv(Xp.flatten(), [Xpt, 3 * count])

                # Reshape the point array
                Xpt = Xpt.reshape(-1, 3)

                # Asymptotic order of accuracy on per-element basis
                s = 1.0
                if args.order >= 3:
                    s = args.order - 1.0

                # Dimension of the problem
                d = 2.0

                # Set the exponent
                exponent = d / (d + s)

                # Compute the target error as a fixed fraction of the
                # error estimate. This will result in the size of the
                # mesh increasing at each iteration.
                if args.remesh_strategy == "fraction":
                    err_target = 0.1 * err_est
                else:
                    # Set a fixed target error
                    err_target = 1e-4  # Target error estimate

                # Set the error estimate
                cval = 1.0
                if args.remesh_strategy == "fixed_mesh":
                    # Compute the constant for element count
                    cval = (element_count_target) ** (-1.0 / d)
                    cval *= (np.sum(errors ** (exponent))) ** (1.0 / d)
                else:
                    # Compute the constant for target error
                    cval = (err_target / np.sum(errors ** (exponent))) ** (1.0 / s)

                # Compute the element-wise target error
                hvals = cval * errors ** (-(1.0 / (d + s)))

                # Set the values of
                if feature_size is not None:
                    for i, hp in enumerate(hvals):
                        hlocal = feature_size.getFeatureSize(Xpt[i, :])
                        hvals[i] = np.min(
                            (np.max((hp * hlocal, 0.25 * hlocal)), 2 * hlocal)
                        )
                else:
                    for i, hp in enumerate(hvals):
                        hvals[i] = np.min(
                            (np.max((hp * htarget, 0.25 * htarget)), 2 * htarget)
                        )

                # Allocate the feature size object
                hmax = 10.0
                hmin = 0.1
                feature_size = TMR.PointFeatureSize(
                    Xpt, hvals, hmin, hmax, num_sample_pts=12
                )
            else:
                size = error.shape[0]
                comm.gather(size, root=root)
                comm.Gatherv(error, None)
                comm.Gatherv(Xp.flatten(), None)

                # Create a dummy feature size object...
                feature_size = TMR.ConstElementSize(0.5 * htarget)

            # Create the surface mesh
            mesh.mesh(fs=feature_size, opts=opts)

            # Create the corresponding mesh topology from the mesh-model
            model = mesh.createModelFromMesh()
            topo = TMR.Topology(comm, model)

            # Create the quad forest and set the topology of the forest
            depth = 0
            if order == 2:
                depth = 1
            forest = TMR.QuadForest(comm)
            forest.setTopology(topo)
            forest.setMeshOrder(order, TMR.UNIFORM_POINTS)
            forest.createTrees(depth)
        else:
            # Allocate the refinement array
            refine = np.zeros(len(error), dtype=np.intc)

            # Determine the cutoff values
            cutoff = bins[-1]
            bin_sum = 0
            for i in range(len(bins) + 1):
                bin_sum += bins[i]
                if bin_sum > 0.15 * ntotal:
                    cutoff = bounds[i + 1]
                    break

            log_cutoff = np.log(cutoff)

            # Element target error is still too high. Adapt based solely
            # on decreasing the overall error
            nrefine = 0
            for i, err in enumerate(error):
                # Compute the log of the error
                logerr = np.log(err)

                if logerr > log_cutoff:
                    refine[i] = 1
                    nrefine += 1

            # Refine the forest
            forest.refine(refine)
            forest.balance(1)
            forest.repartition()
