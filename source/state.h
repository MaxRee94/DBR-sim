#pragma once
#include <iostream>
#include "agents.h"
#include "grid.h"


class State {
public:
	State() {
		grid = Grid();
		population = Population();
	}
	State(int gridsize, float cellsize, float max_radius, float radius_q1, float radius_q2) {
		population = Population(max_radius, cellsize, radius_q1, radius_q2);
		grid = Grid(gridsize, cellsize);
	}
	void repopulate_grid(int verbosity) {
		grid.reset();
		for (auto& tree : population.members) {
			grid.populate_tree_domain(&tree);
		}
		if (verbosity == 2) cout << "Repopulated grid." << endl;
	}
	void set_tree_cover(float _tree_cover) {
		help::init_RNG();
		grid.reset();
		while (grid.get_tree_cover() < _tree_cover) {
			pair<float, float> position = grid.get_random_real_position();
			Tree* tree = population.add(position);
			grid.populate_tree_domain(tree);
			if (population.size() % 1000 == 0) {
				printf("Current tree cover: %f, current population size: %i\n", grid.get_tree_cover(), population.size());
			}
			continue;
		}
		printf("Final tree cover: %f\n", grid.get_tree_cover());

		int no_small = 0;
		for (auto& tree : population.members) {
			if (tree.radius < population.max_radius / 2.0) {
				no_small++;
			}
		}
		printf("- Fraction small trees: %f \n", (float)no_small / (float)population.size());
	}
	Grid grid;
	Population population;
};

