import csv
import os
from pathlib import Path
import datetime
import cv2
from config import *
import numpy as np



EXPORT_LOCATION = r"F:/Development/DBR-sim/data_out/state data"


def export_state(dynamics, path="", init_csv=True, control_variable=None, control_value=None):
    fieldnames = ["initial tree cover", "time", "tree cover", "population size", "#seeds spread", "fire mean spatial extent"]
    if control_variable:
        fieldnames.insert(0, control_variable)
    if init_csv and not os.path.exists(path):
        if path == "":
            path = os.path.join(EXPORT_LOCATION, "Simulation_" + str(datetime.datetime.now()).replace(":", "-") + ".csv")
        with open(path, 'w', newline='') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()

    with open(path, 'a', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        result = {
                "initial tree cover": str(dynamics.state.initial_tree_cover),
                "time": str(dynamics.time),
                "tree cover": str(dynamics.state.grid.get_tree_cover()), 
                "population size": str(dynamics.state.population.size()),
                "#seeds spread": str(dynamics.seeds_dispersed),
                "fire mean spatial extent": str(dynamics.fire_spatial_extent)
            }
        if control_variable:
            result[control_variable] = control_value
        writer.writerow(result)
        
    return path


def import_image(fpath):
    img = cv2.imread(fpath)
    return img

def save_numpy_array_to_file(array, path):
    np.save(path, array)

def save_tree_positions(dynamics):
    tree_positions = dynamics.state.get_tree_positions()
    time = str(dynamics.time).zfill(4)
    save_numpy_array_to_file(tree_positions, f"{DATA_OUT_DIR}/tree_positions/tree_positions_iter_{time}.npy")




