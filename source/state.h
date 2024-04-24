#pragma once
#include <iostream>
#include "agents.h"
#include "grid.h"


class State {
public:
	State() {
		grid = Grid();
		population = Population();
		init_neighbor_offsets();
	}
	State(
		int gridsize, float cellsize, float max_radius, float radius_q1, float radius_q2,
		float _seed_bearing_threshold, float _mass_budget_factor, float _seed_mass, float _saturation_threshold,
		map<string, map<string, float>> &strategy_distribution_params
	) {
		seed_bearing_threshold = _seed_bearing_threshold;
		mass_budget_factor = _mass_budget_factor / help::cubed(max_radius); // Normalize by maximum radius
		seed_mass = _seed_mass;
		saturation_threshold = _saturation_threshold;
		population = Population(
			max_radius, cellsize, radius_q1, radius_q2, mass_budget_factor, strategy_distribution_params
		);
		grid = Grid(gridsize, cellsize);
		init_neighbor_offsets();
	}
	void init_neighbor_offsets() {
		neighbor_offsets = new pair<int, int>[8];
		int q = 0;
		for (int i = -1; i < 2; i++) {
			for (int j = -1; j < 2; j++) {
				if (i == 0 && j == 0) continue;
				neighbor_offsets[q] = pair<int, int>(i, j);
				q++;
			}
		}
	}
	void repopulate_grid(int verbosity) {
		if (verbosity == 2) cout << "Repopulating grid..." << endl;
		grid.reset();
		for (auto& [id, tree] : population.members) {
			if (id == -1 || tree.id == -1) population.remove(id); // HOTFIX: Sometimes trees are not initialized properly and need to be removed.
			grid.populate_tree_domain(&tree);
			tree.update_life_phase(population.max_radius, seed_bearing_threshold);
		}
		if (verbosity == 2) cout << "Repopulated grid." << endl;
	}
	float get_dist(pair<float, float> a, pair<float, float> b, bool verbose = false) {
		vector<float> dists = {help::get_dist(a, b)};
		for (int i = 0; i < 8; i++) {
			float dist = help::get_dist(
				neighbor_offsets[i] * grid.width_r + b, a
			);
			dists.push_back(dist);
		}
		if (verbose) printf("normal dist: %f, periodic dist: %f \n", dists[0], help::get_min(&dists));
		return help::get_min(&dists);
	}
	vector<Tree*> get_tree_neighbors(pair<float, float> baseposition, float search_radius, int base_id = -1) {
		vector<Tree*> neighbors;
		for (auto& [id, candidate] : population.members) {
			if (base_id != -1 && id == base_id) {
				continue;
			}
			if (get_dist(candidate.position, baseposition) < search_radius) {
				neighbors.push_back(&candidate);
			}
		}
		return neighbors;
	}
	vector<Tree*> get_tree_neighbors(Tree* base) {
		vector<Tree*> neighbors;
		float search_radius = (round(base->radius * 1.1) + population.max_radius);
		return get_tree_neighbors(base->position, search_radius, base->id);
	}
	void set_cover_from_image(float* image, int img_width, int img_height) {
		float pixel_size = grid.width_r / (float)img_width;
		int no_gridcells_along_x_per_pixel = round(grid.width_r / img_width);
		pair<int, int> bb = pair<int, int>(no_gridcells_along_x_per_pixel, no_gridcells_along_x_per_pixel);
		for (int x = 0; x < img_width; x++) {
			for (int y = 0; y < img_height; y++) {
				float pixel_value = image[y * img_width + x];
				pair<int, int> pixel_position(x, y);
				pair<int, int> grid_bb_min = no_gridcells_along_x_per_pixel * pixel_position;
				pair<int, int> grid_bb_max = grid_bb_min + bb;
				float cur_cover = grid.get_tree_cover_within_bb(grid_bb_min, grid_bb_max);
				if (cur_cover < pixel_value) {
					pair<float, float> position = grid.get_random_location_within_cell(grid_bb_min);
					Tree* tree = population.add(position);
					grid.populate_tree_domain(tree);
				}
			}
		}
	}
	void set_tree_cover(float _tree_cover) {
		help::init_RNG();
		grid.reset();

		int wind_trees = 0;
		int animal_trees = 0;
		while (grid.get_tree_cover() < _tree_cover) {
			pair<float, float> position = grid.get_random_real_position();
			Tree* tree = population.add(position);
			if (tree->id == -1) population.remove(population.no_created_trees - 1); // HOTFIX: Sometimes trees are not initialized properly and need to be removed.
			grid.populate_tree_domain(tree);
			if (population.get_strat(tree->id)->vector == "wind") {
				wind_trees++;
			}
			else if (population.get_strat(tree->id)->vector == "animal") {
				animal_trees++;
			}

			if (population.size() % 1000 == 0) {
				printf("Current tree cover: %f, current population size: %i\n", grid.get_tree_cover(), population.size());
			}
			continue;
		}
		printf("Final tree cover: %f\n", grid.tree_cover);
		printf("Wind trees: %i, Animal trees: %i\n", wind_trees, animal_trees);
		printf("First tree's strategy: \n");
		population.get_strat(10)->print();

		// Count no small trees
		int no_small = 0;
		for (auto& [id, tree] : population.members) {
			if (tree.radius < population.max_radius / 2.0) {
				no_small++;
			}
		}
		printf("- Fraction small trees: %f \n", (float)no_small / (float)population.size());

		initial_tree_cover = grid.tree_cover;
		repopulate_grid(0);
	}
	Grid grid;
	Population population;
	pair<int, int>* neighbor_offsets = 0;
	float seed_bearing_threshold = 0;
	float initial_tree_cover = 0;
	float mass_budget_factor = 0;
	float seed_mass = 0;
	float saturation_threshold = 0;
};

