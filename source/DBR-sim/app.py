import sys

import cv2
import visualization as vis
import numpy as np
import time

sys.path.append(r"F:\Development\DBR-sim\build")
from x64.Release import dbr_cpp as cpp



def init(timestep=None, gridsize=None, cellsize=None, mean_radius=None, treecover=None, **_):
    cpp.check_communication()
    dynamics = cpp.Dynamics(timestep)
    dynamics.init_state(gridsize, cellsize, mean_radius)
    dynamics.state.set_tree_cover(treecover)
    
    return dynamics

def updateloop(dynamics, **kwargs):
    start = time.time()
    is_within_timelimit = True;
    while is_within_timelimit:
        dynamics.update()
        vis.visualize(dynamics.state.grid, kwargs["image_width"])
        is_within_timelimit = time.time() - start < kwargs["timelimit"]

    if not is_within_timelimit:
        print(f"Simulation terminated due to timelimit ({kwargs["timelimit"]} s) expiration.")
    cv2.destroyAllWindows()

def main(**kwargs):
    dynamics = init(**kwargs)
    updateloop(dynamics, **kwargs)
    

