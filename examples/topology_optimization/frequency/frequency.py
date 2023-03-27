"""
Compliance minimization with mass and frequency constraints

This example demonstrates:

1) Creating meshes using TMR.Creator methods
2) Setting up a topology optimization problem with frequency constraints
3) Design-feature based AMR
4) Use of a Lagrange filter

For a full list of arguments type:

python frequency.py --help
"""

from __future__ import print_function
from mpi4py import MPI
from tmr import TMR, TopOptUtils
from paropt import ParOpt
from tacs import TACS, elements, constitutive, functions
import numpy as np
import argparse
import os

# Import the cantilever example set up
import sys

sys.path.append("../cantilever/")
from cantilever import OctCreator, CreatorCallback, create_forest, create_problem

# Create an argument parser to read in arguments from the commnad line
p = argparse.ArgumentParser()
# Output options
p.add_argument("--prefix", type=str, default="./results")
p.add_argument("--output_freq", type=int, default=1)

# Mesh parameters
p.add_argument("--htarget", type=float, default=5.0)
p.add_argument("--init_depth", type=int, default=4)
p.add_argument("--order", type=int, default=2)
p.add_argument("--refine_flag", type=bool, nargs="+", default=[True])

# Solver parameters
p.add_argument("--mg_levels", type=int, nargs="+", default=[2])
p.add_argument("--use_decrease_order", action="store_true", default=True)

# Frequency constraint parameters
p.add_argument("--min_freq", type=float, default=3.5)
p.add_argument("--num_eigs", type=int, default=9)
p.add_argument("--num_recycle", type=int, default=9)
p.add_argument("--use_jd", action="store_true", default=False)

# Optimizer settings
p.add_argument("--max_opt_iters", type=int, nargs="+", default=[50])
p.add_argument("--opt_abs_tol", type=float, default=1e-6)
p.add_argument("--opt_barrier_frac", type=float, default=0.25)
p.add_argument("--opt_barrier_power", type=float, default=1.0)
p.add_argument("--qn_subspace", type=int, default=2)
p.add_argument("--tr_penalty", type=float, default=15.0)

# Optimization parameters
p.add_argument("--q_penalty", type=float, nargs="+", default=[8.0])
p.add_argument("--ks_weight", type=float, nargs="+", default=[25.0])
p.add_argument("--eps", type=float, nargs="+", default=[0.1])

args = p.parse_args()

# Set the communicator
comm = MPI.COMM_WORLD

# Print out all of the arguments to the command line
if comm.rank == 0:
    for arg in vars(args):
        print("%-20s" % (arg), getattr(args, arg))

# Set the optimization parameters
optimization_options = {
    "algorithm": "tr",
    # Parameters for the trust region method
    "tr_init_size": 0.01,
    "tr_max_size": 0.05,
    "tr_min_size": 1e-6,
    "tr_eta": 0.25,
    "tr_max_iterations": 50,
    "tr_penalty_gamma": args.tr_penalty,
    # Parameters for the interior point method (used to solve the
    # trust region subproblem)
    "init_barrier_param": 10.0,
    "qn_subspace_size": args.qn_subspace,
    "abs_res_tol": 1e-8,
    "norm_type": "l1",
    "barrier_strategy": "monotone",
    "start_strategy": "affine_step",
}

prefix = args.prefix
optimization_options["output_file"] = os.path.join(prefix, "output_file.dat")
optimization_options["tr_output_file"] = os.path.join(prefix, "tr_output_file.dat")

# Set the max nmber of outer iterations
# (for continuation strategies and/or mesh refinement)
mg_levels = args.mg_levels
max_iterations = len(mg_levels)

# Set the material properties
material_properties = constitutive.MaterialProperties(
    rho=2600.0, E=79e9, nu=0.3, ys=350e6
)

# Create the stiffness properties object
props = TMR.StiffnessProperties(material_properties, k0=1e-3, eps=0.2, q=5.0)

# Set the boundary conditions for the problem
bcs = TMR.BoundaryConditions()
bcs.addBoundaryCondition("fixed", [0, 1, 2], [0.0, 0.0, 0.0])

# Create the initial forest
forest = create_forest(
    comm, args.init_depth, htarget=args.htarget, filename="../cantilever/cantilever.stp"
)

# Set the original filter to NULL
orig_filter = None
xopt = None
iter_offset = 0

# Start the optimization
for step in range(max_iterations):
    # Create the TopoProblem instance
    nlevels = mg_levels[step]
    problem = create_problem(
        forest, bcs, props, nlevels, vol_frac=0.4, iter_offset=iter_offset
    )
    iter_offset += args.max_opt_iters[step]

    # Add the natural frequency constraint
    min_freq = args.min_freq
    freq_opts = {
        "use_jd": args.use_jd,
        "num_eigs": args.num_eigs,
        "num_recycle": args.num_recycle,
        "track_eigen_iters": nlevels,
    }
    TopOptUtils.addNaturalFrequencyConstraint(problem, min_freq, **freq_opts)

    problem.initialize()
    problem.setPrefix(args.prefix)

    # Extract the filter to interpolate design variables
    filtr = problem.getFilter()

    if orig_filter is not None:
        # Create one of the new design vectors
        x = problem.createDesignVec()
        TopOptUtils.interpolateDesignVec(orig_filter, xopt, filtr, x)
        problem.setInitDesignVars(x)

    # Set the new original filter
    orig_filter = filtr

    # Check the problem gradients
    problem.checkGradients(1e-6)

    # Set the max number of iterations
    optimization_options["tr_max_iterations"] = args.max_opt_iters[step]

    # Optimize the problem
    opt = TopOpt.Optimizer(problem, optimization_options)
    opt.optimize()
    xopt, z, zw, zl, zu = opt.getOptimizedPoint()

    # Refine based solely on the value of the density variable
    assembler = problem.getAssembler()
    forest = forest.duplicate()

    # Perform the density-based refinement
    lower = 0.05
    upper = 0.5
    TopOptUtils.densityBasedRefine(forest, assembler, lower=lower, upper=upper)

    # Repartition the mesh
    forest.repartition()

    # Output for visualization
    flag = TACS.OUTPUT_CONNECTIVITY | TACS.OUTPUT_NODES | TACS.OUTPUT_EXTRAS
    f5 = TACS.ToFH5(assembler, TACS.SOLID_ELEMENT, flag)
    f5.writeToFile("beam{0}.f5".format(step))
