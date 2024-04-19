#pragma once
#include "dispersal.h"


class Dynamics {
public:
	Dynamics() = default;
	Dynamics(
		int _timestep, float _cellsize, float _self_ignition_factor, float _rainfall, float _seed_bearing_threshold, float _mass_budget_factor,
		float _growth_rate_multiplier, float _unsuppressed_flammability, float _min_suppressed_flammability, float _max_suppressed_flammability,
		float _radius_suppr_flamm_min, float radius_range_suppr_flamm, float _max_radius, float _saturation_threshold, float _fire_resistance_argmin,
		float _fire_resistance_argmax, float _fire_resistance_stretch, float _background_mortality, map<string, map<string, float>> _strategy_distribution_params,
		int _verbosity
	) :
		timestep(_timestep), cellsize(_cellsize), unsuppressed_flammability(_unsuppressed_flammability),
		self_ignition_factor(_self_ignition_factor), rainfall(_rainfall), seed_bearing_threshold(_seed_bearing_threshold),
		mass_budget_factor(_mass_budget_factor), growth_rate_multiplier(_growth_rate_multiplier),
		radius_suppr_flamm_min(_max_radius * _radius_suppr_flamm_min),
		flamm_d_radius((_cellsize * _min_suppressed_flammability - _cellsize * _max_suppressed_flammability) / (_max_radius * radius_range_suppr_flamm)),
		max_suppressed_flammability(_cellsize * _max_suppressed_flammability),
		min_suppressed_flammability(_cellsize * _min_suppressed_flammability),
		max_radius(_max_radius), verbosity(_verbosity), saturation_threshold(1.0f / _saturation_threshold), fire_resistance_argmin(_fire_resistance_argmin),
		fire_resistance_argmax(_fire_resistance_argmax), fire_resistance_stretch(_fire_resistance_stretch), background_mortality(_background_mortality),
		strategy_distribution_params(_strategy_distribution_params)
	{
		time = 0;
		help::init_RNG();
		pop = &state.population;
		grid = &state.grid;
	};
	void init_state(int gridsize, float radius_q1, float radius_q2, float _seed_mass) {
		state = State(
			gridsize, cellsize, max_radius, radius_q1, radius_q2, seed_bearing_threshold, mass_budget_factor,
			_seed_mass, saturation_threshold, strategy_distribution_params
		);
		linear_disperser = Disperser(&state);
		wind_disperser = WindDispersal(&state);
		animal_dispersal = AnimalDispersal(&state);
		neighbor_offsets = state.neighbor_offsets;
	}
	void update() {
		// Prepare next iteration
		time++;
		printf("Time: %i\n", time);
		grid->reset_state_distr();

		// Do simulation
		disperse();
		burn();
		grow();
		induce_background_mortality();

		// Do post-simulation cleanup and data reporting
		state.repopulate_grid(0);
		grid->redo_count();
		if (verbosity > 0) report_state();
	}
	void report_state() {
		printf("- Tree cover: %f, #trees: %i \n", grid->get_tree_cover(), pop->size());
		if (time == 1 && global_kernels.size() != 0 && verbosity > 0) {
			Crop* crop = pop->get_crop(1);
			printf("total biomass/tree: %f \n", crop->mass);
			printf("seed mass: %f \n", crop->seed_mass);
			printf("no seeds: %i \n", crop->no_seeds);
		}
		if (verbosity == 2) for (auto& [id, tree] : pop->members) if (id % 500 == 0) printf("Radius of tree %i : %f \n", id, tree.radius);
	}
	vector<float> get_ordered_fire_ignition_times() {
		int i = 0;
		int fire_count = round(self_ignition_factor * rainfall * (float)grid->no_savanna_cells / (float)1e6);
		vector<float> fire_ignition_times = {};
		while (i < fire_count) {
			float t_start = help::get_rand_float(0.0, 1.0) + time;
			fire_ignition_times.push_back(t_start);
			i++;
		}
		std::sort(fire_ignition_times.begin(), fire_ignition_times.end());
		return fire_ignition_times;
	}
	void grow_tree(Tree &tree) {
		// TEMP: constant growth rate. TODO: Make dependent on radius and life phase (resprout or seedling)
		tree.radius_tmin1 = tree.radius;
		tree.radius = min(tree.radius + sqrtf(tree.radius) * growth_rate_multiplier, pop->max_radius);
	}
	void grow() {
		for (auto& [id, tree] : pop->members) {
			grow_tree(tree);
		}
	}
	void set_global_linear_kernel(float lin_diffuse_q1, float lin_diffuse_q2, float min, float max) {
		global_kernels["linear"] = Kernel(1, lin_diffuse_q1, lin_diffuse_q2, min, max);
		cout << "Global kernel created (Linear diffusion). " << endl;
	}
	void set_global_wind_kernel(float wspeed_gmean, float wspeed_stdev, float seed_tspeed, float abs_height) {
		global_kernels["wind"] = Kernel(1, grid->width_r, wspeed_gmean, wspeed_stdev, seed_tspeed, abs_height);
		cout << "Global kernel created (Wind dispersal). " << endl;
	}
	void set_global_animal_kernel(map<string, map<string, float>>animal_kernel_params) {
		global_kernels["animal"] = Kernel(1, "animal");
		animals = Animals(&state, animal_kernel_params);
		cout << "Global kernel created (Dispersal by animals). \n";
	}
	void set_global_kernels(map<string, map<string, float>> nonanimal_kernel_params, map<string, map<string, float>> animal_kernel_params) {
		map<string, float> params = nonanimal_kernel_params["linear"];
		set_global_linear_kernel(params["lin_diffuse_q1"], params["lin_diffuse_q2"], params["min"], params["max"]);
		params = nonanimal_kernel_params["wind"];
		set_global_wind_kernel(params["wspeed_gmean"], params["wspeed_stdev"], params["seed_tspeed"], params["abs_height"]);
		set_global_animal_kernel(animal_kernel_params);
	}
	bool does_global_kernel_exist(string type) {
		return global_kernels.find(type) != global_kernels.end();
	}
	void ensure_crop_has_kernel(Crop* crop) {
		if (crop->kernel == nullptr) {
			string tree_dispersal_vector = pop->get_strat(crop->id)->vector;
			if (does_global_kernel_exist(tree_dispersal_vector))
				crop->kernel = pop->add_kernel(crop->id, global_kernels[tree_dispersal_vector]);
			else {
				printf("No global kernel found for tree dispersal vector %s. \n", tree_dispersal_vector.c_str());
				exit(1);
			}
		}
	}
	void disperse() {
		int x = 0;
		int j = 0;
		int pre_dispersal_popsize = pop->size();
		for (auto& [id, tree] : pop->members) {
			if (tree.life_phase < 2) continue;
			x++;
			Crop* crop = pop->get_crop(id);
			ensure_crop_has_kernel(crop);
			crop->update(tree);
			if (x == 1) printf("Crop mass: %f, diaspore mass: %f \n", crop->mass, crop->strategy.diaspore_mass);
			for (int i = 0; i < crop->no_diaspora; i++) {
				if (crop->kernel->type == "wind") {
					wind_disperser.disperse(crop);
				}
				else if (crop->kernel->type == "animal") {
					fruits.add_fruit(crop);
				}
				else {
					linear_disperser.disperse(crop);
				}
			}
			j += crop->no_seeds;
		}
		if (fruits.are_available()) {
			animal_dispersal.disperse(animals, fruits);
		}
		if (verbosity > 0) printf("Number of seed bearing trees: %i, #seeds (all): %i, #germinated seeds: %i \n", x, j, pop->size() - pre_dispersal_popsize);
		seeds_dispersed = j;
	}
	void induce_background_mortality() {
		for (auto& [id, tree] : pop->members) {
			if (help::get_rand_float(0, 1) < background_mortality) {
				kill_tree(&tree);
			}
		}
	}
	void burn() {
		if (verbosity == 2) printf("Updated tree flammabilities.\n");
		vector<float> fire_ignition_times = get_ordered_fire_ignition_times();
		int no_burned_cells = 0;
		int re_ignitions = 0;
		fire_spatial_extent = 0;
		for (int i = 0; i < fire_ignition_times.size(); i++) {
			Cell* sav_cell = grid->get_random_savanna_cell();
			int _no_burned_cells = percolate(sav_cell, fire_ignition_times[i]);
			no_burned_cells += _no_burned_cells;
			if (_no_burned_cells <= 1 && re_ignitions < 5) { // If fire did not spread beyond ignition point, re-do percolation.
				re_ignitions++;
				i--; continue;
			}
			fire_spatial_extent += no_burned_cells * (grid->cellsize * grid->cellsize);
		}
		fire_spatial_extent /= fire_ignition_times.size();
		if (verbosity > 0) {
			printf("Cells burned: %i \n", no_burned_cells);
			printf("Number of fires: %i \n", fire_ignition_times.size());
		}
	}
	float get_forest_flammability(Cell* cell, float fire_free_interval) {
		float d_radius = max(cell->cumulative_radius - radius_suppr_flamm_min, 0);
		float flammability = max(max_suppressed_flammability + d_radius * flamm_d_radius, min_suppressed_flammability) / cell->cumulative_radius;
		return flammability;
	}
	float get_savanna_flammability(float fire_free_interval) {
		return fire_free_interval * unsuppressed_flammability;
	}
	float get_cell_flammability(Cell* cell, float fire_free_interval) {
		if (cell->state == 1) {
			return get_forest_flammability(cell, fire_free_interval);
		}
		else return get_savanna_flammability(fire_free_interval);
	}
	float get_stem_diameter(float crown_diameter) {
		return pow(10.0f, (log10(crown_diameter) + 0.12) / 0.63);
	}
	float get_bark_thickness(float stem_diameter) {
		return 0.31 * pow(stem_diameter, 1.276);
	}
	bool tree_dies(Tree* tree, float fire_free_interval) {
		float stem_diameter = get_stem_diameter(tree->radius * 2.0f);
		float bark_thickness = get_bark_thickness(stem_diameter);
		float survival_probability = help::get_sigmoid(bark_thickness, fire_resistance_argmin, fire_resistance_argmax, fire_resistance_stretch);
		if (verbosity == 2) printf("stem diameter: %f cm, bark thickness: %f mm, survival probability: %f \n", stem_diameter, bark_thickness, survival_probability);
		return help::get_rand_float(0.0f, 1.0f) > survival_probability; 
		// COMMENT: We currently assume topkill always implies death, but resprouting should also be possible. (TODO: make death dependent on fire-free interval)
	}
	void kill_tree(Tree* tree, float time_last_fire, queue<Cell*>& queue) {
		if (verbosity == 2) printf("Killing tree %i ... \n", tree->id);
		grid->burn_tree_domain(tree, queue, time_last_fire);
		state.population.remove(tree);
	}
	void kill_tree(Tree* tree) {
		if (verbosity == 2) printf("Killing tree %i ... \n", tree->id);
		grid->kill_tree_domain(tree);
		state.population.remove(tree);
	}
	void induce_tree_mortality(Cell* cell, float fire_free_interval, queue<Cell*>& queue) {
		for (auto [tree_id, _] : cell->trees) {
			Tree* tree = pop->get(tree_id);
			if (tree->last_mortality_check == time) continue; // Skip mortality evaluation if this was already done in the current timestep.
			if (tree_dies(tree, fire_free_interval)) {
				kill_tree(tree, cell->time_last_fire, queue);
			}
			tree->last_mortality_check = time;
		}
	}
	void kill_seedlings(Cell* cell) {
		for (auto [tree_id, _] : cell->trees) {
			pop->remove(tree_id);
			cell->trees.erase(tree_id);
		}
	}
	inline bool cell_will_ignite(Cell* cell, float t_start) {
		if (t_start - cell->time_last_fire < 10e-4) {
			return false; // Do not ignite cells which have already been burned by the current fire.
		}
		return help::get_rand_float(0.0, 1.0) < get_cell_flammability(cell, min(t_start - cell->time_last_fire, 1));
	}
	inline void burn_cell(Cell* cell, float t_start, queue<Cell*> &queue) {
		cell->time_last_fire = t_start;
		grid->state_distribution[grid->pos_2_idx(cell->pos)] = -5;
		if (cell->state == 1) {
			induce_tree_mortality(cell, t_start - cell->time_last_fire, queue);
		}
		else {
			kill_seedlings(cell);
		}
	}
	int percolate(Cell* cell, float t_start) {
		std::queue<Cell*> queue;
		queue.push(cell);
		burn_cell(cell, t_start, queue);
		int no_burned_cells = 1;
		if (verbosity == 2) printf("Percolating fire...\n");
		pop_size = state.population.size();
		while (!queue.empty()) {
			Cell* cell = queue.front();
			queue.pop();

			// Percolate to neighbors
			for (int i = 0; i < 8; i++) {
				Cell* neighbor = grid->get_cell_at_position(cell->pos + neighbor_offsets[i]);
				if (cell_will_ignite(neighbor, t_start)) {
					queue.push(neighbor);
					burn_cell(neighbor, t_start, queue);
					no_burned_cells++;
				}
			}
		}
		return no_burned_cells;
	}
	float* get_firefree_intervals() {
		float* histo = new float[grid->no_cells];
		for (int i = 0; i < grid->no_cells; i++) {
			float interval = time - grid->distribution[i].time_last_fire;
			histo[i] = interval;
		}
		return histo;
	}
	float unsuppressed_flammability = 0;
	float min_suppressed_flammability = 0;
	float max_suppressed_flammability = 0;
	float mass_budget_factor = 0;
	float self_ignition_factor = 0;
	float rainfall = 0;
	float seed_bearing_threshold = 0;
	float growth_rate_multiplier = 0;
	float radius_suppr_flamm_min = 0;
	float flamm_d_radius = 0;
	float max_radius = 0;
	float cellsize = 0;
	float fire_spatial_extent = 0;
	float saturation_threshold = 0;
	float fire_resistance_argmin = 0;
	float fire_resistance_argmax = 0;
	float fire_resistance_stretch = 0;
	float background_mortality = 0;
	int timestep = 0;
	int time = 0;
	int pop_size = 0;
	int verbosity = 0;
	int seeds_dispersed = 0;
	State state;
	Population* pop = 0;
	Grid* grid = 0;
	Disperser linear_disperser;
	WindDispersal wind_disperser;
	AnimalDispersal animal_dispersal;
	pair<int, int>* neighbor_offsets = 0;
	map<string, Kernel> global_kernels;
	map<string, map<string, float>> strategy_distribution_params;
	Fruits fruits;
	Animals animals;
};

