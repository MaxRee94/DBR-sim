#pragma once
#include "diaspora.h"



class ResourceCell {
public:
	ResourceCell() = default;
	ResourceCell(pair<int, int> _position, int _idx): pos(_position), idx(_idx) {}
	void reset() {
		fruits.clear();
	}
	bool extract_random_fruit(Fruit &fruit, vector<int>& compute_times) {
		auto start = high_resolution_clock::now();
		if (fruits.no_fruits() == 0) {
			return false;
		}
		compute_times[12] += help::microseconds_elapsed_since(start);
		bool success = fruits.get(fruit, compute_times);
		return success;
	}
	float get_fruit_abundance_index() {
		if (fruits.no_fruits() == 0) return 0.0f;
		return log10(fruits.no_fruits()); // Fruit Abundance Index, as used by Morales et al 2013.
	}
	pair<int, int> pos;
	pair<int, int> grid_bb_min;
	pair<int, int> grid_bb_max;
	int idx = 0;
	Fruits fruits;
};


class ResourceGrid : public Grid {
public:
	ResourceGrid() = default;
	ResourceGrid(State* _state, int _width, float _cell_width, vector<string> _species, map<string, map<string, float>>& _animal_kernel_params): Grid(_width, _cell_width) {
		width = _width;
		state = _state;
		grid = &state->grid;
		width_r = (float)width * cell_width;
		size = width * width;
		lookup_table_size = size * size;
		cells = new ResourceCell[size];
		selection_probabilities = DiscreteProbabilityModel(size);
		species = _species;
		animal_kernel_params = _animal_kernel_params;
		init_property_distributions(species);
		init_cells();
		init_neighbor_offsets();
	}
	void free() {
		delete[] cells;
		delete_c();
		delete_f();
		delete[] d;
		delete[] cover;
		delete[] fruit_abundance;
		delete[] dist_aggregate;
		delete[] color_distribution;
		delete[] visits;
		selection_probabilities.free();
		Grid::free();
	}
	void delete_c() {
		for (auto it = c.begin(); it != c.end(); it++) {
			delete[] it->second;
		}
	}
	void delete_f() {
		for (auto it = f.begin(); it != f.end(); it++) {
			delete[] it->second;
		}
	}
	void init_property_distributions(vector<string> &species) {
		d = new float[size];
		cover = new float[size];
		fruit_abundance = new float[size];
		dist_aggregate = new float[size];
		color_distribution = new int[size];
		visits = new int[size];
		for (int i = 0; i < size; i++) visits[i] = 0;
		for (int i = 0; i < size; i++) dist_aggregate[i] = 0;
		for (int i = 0; i < species.size(); i++) {
			c[species[i]] = new float[size];
			f[species[i]] = new float[size];
			dist_lookup_table[species[i]] = new float[lookup_table_size];
		}
	}
	void reset() {
		for (int i = 0; i < size; i++) {
			cells[i].reset();
		}
		has_fruits = false;
		total_no_fruits = 0;
	}
	void add_crop(pair<float, float> position, Crop* crop) {
		ResourceCell* cell = get_resource_cell_at_position(position);
		cell->fruits.add_fruits(crop);
		total_no_fruits += crop->fruit_abundance;
		has_fruits = total_no_fruits > 0;
	}
	float get_tree_cover_within_resourcegrid_bb(pair<int, int> bb_min, pair<int, int> bb_max) {
		int no_forest_cells = 0;
		int no_cells = 0;
		for (int x = bb_min.first; x < bb_max.first; x++) {
			for (int y = bb_min.second; y < bb_max.second; y++) {
				Cell* cell = grid->get_cell_at_position(pair<int, int>(x, y));
				no_forest_cells += cell->state;
				no_cells++;
			}
		}
		return (float)no_forest_cells / (float)no_cells;
	}
	void compute_cover_and_fruit_abundance() {
		for (int i = 0; i < size; i++) {
			ResourceCell* cell = &cells[i];
			cover[i] = get_tree_cover_within_resourcegrid_bb(cell->grid_bb_min, cell->grid_bb_max);
			cover[i] = asin(sqrt(cover[i]));
			fruit_abundance[i] = cell->get_fruit_abundance_index();
		}
	}
	void init_neighbor_offsets() {
		neighbor_offsets = new pair<float, float>[8];
		int q = 0;
		for (int i = -1; i < 2; i++) {
			for (int j = -1; j < 2; j++) {
				if (i == 0 && j == 0) continue;
				neighbor_offsets[q] = pair<float, float>(i, j);
				q++;
			}
		}
	}
	void get_random_stategrid_location(pair<float, float>& location) {
		get_random_location_within_cell(location);
		location = state->grid.get_gridbased_position(location);
		location = state->grid.cell_width * location; // Convert to real position, at the origin of a stategrid-cell.
	}
	void update_fruit_abundance(ResourceCell* cell, string species, map<string, float> &species_params) {
		fruit_abundance[cell->idx] = cell->get_fruit_abundance_index();
		f[species][cell->idx] = tanh(pow((fruit_abundance[cell->idx] / species_params["a_f"]), species_params["b_f"]));
	}
	void update_fruit_abundance(pair<float, float> position, string species, map<string, float> &species_params) {
		ResourceCell* cell = get_resource_cell_at_position(position);
		update_fruit_abundance(cell, species, species_params);
	}
	bool extract_fruit(pair<int, int> pos, Fruit &fruit, vector<int>& compute_times) {
		auto start = high_resolution_clock::now();
		ResourceCell* cell = get_resource_cell_at_position(pos);
		compute_times[8] += help::microseconds_elapsed_since(start);

		start = high_resolution_clock::now();
		bool success = cell->extract_random_fruit(fruit, compute_times);
		compute_times[11] += help::microseconds_elapsed_since(start);
		total_no_fruits -= success;
		return success;
	}
	pair<int, int> _idx_2_pos(int idx) {
		int x = idx % width;
		int y = idx / width;
		return pair<int, int>(x, y);
	}
	void get_random_location_within_cell(ResourceCell* cell, pair<float, float>& deposition_location) {
		deposition_location.first = help::get_rand_float((float)cell->pos.first * cell_width, (float)(cell->pos.first + 1) * cell_width);
		deposition_location.second = help::get_rand_float((float)cell->pos.second * cell_width, (float)(cell->pos.second + 1) * cell_width);
	}
	void get_random_location_within_cell(pair<float, float>& deposition_location) {
		ResourceCell* cell = get_resource_cell_at_position(deposition_location);
		get_random_location_within_cell(cell, deposition_location);
	}
	ResourceCell* get_resource_cell_at_position(pair<int, int> pos) {
		cap(pos);
		return &cells[pos.second * width + pos.first];
	}
	ResourceCell* get_resource_cell_at_position(pair<float, float> _pos) {
		pair<int, int> pos = get_gridbased_position(_pos);
		return get_resource_cell_at_position(pos);
	}
	pair<float, float> get_real_cell_position(ResourceCell* cell) {
		return pair<float, float>(cell->pos.first * cell_width, cell->pos.second * cell_width);
	}
	ResourceCell* get_random_resource_cell() {
		int idx = help::get_rand_int(0, size - 1);
		return &cells[idx];
	}
	float get_resourcegrid_dist(pair<float, float> a, pair<float, float>* b, bool verbose = false) {
		// This function yields the minimum distance between a and b, taking into account periodic boundary conditions.
		// WARNING: Position b may be modified in the process to a position outside of the grid.
		vector<float> dists = { help::get_manhattan_dist(a, *b)};
		float min_dist = dists[0];
		int min_idx = 0;
		for (int i = 0; i < 8; i++) {
			float dist = help::get_manhattan_dist(
				neighbor_offsets[i] * width_r + *b, a
			);

			if (dist < min_dist && dist > 0) {
				min_dist = dist;
				min_idx = i + 1;
			}
			dists.push_back(dist);
		}
		float dist;
		if (min_idx != 0) {
			dist = help::get_dist(neighbor_offsets[min_idx - 1] * width_r + *b, a);
			*b = neighbor_offsets[min_idx - 1] * width_r + *b;
		}
		else dist = help::get_dist(a, *b);
		if (verbose) printf("normal dist: %f, periodic dist: %f \n", dists[0], dist);
		return dist;
	}
	float get_resourcegrid_dist(pair<float, float> a, pair<float, float> b, bool verbose = false) {
		// This function yields the minimum distance between a and b, taking into account periodic boundary conditions.
		// Position b is not modified in the process.
		pair<float, float> _b = b;
		return get_resourcegrid_dist(a, &_b, verbose);
	}
	pair<float, float> get_shortest_trajectory(pair<float, float> a, pair<float, float> _b, float &distance) {
		pair<float, float> b = _b;
		distance = get_resourcegrid_dist(a, &b); // Modify position b if needed to ensure it is at the shortest distance from a.
		//printf("new b: %f, %f. Trajectory: %f, %f \n", b.first, b.second, (b-a).first, (b-a).second);
		return b - a;
	}
	int* get_color_distribution(string species, string collect = "distance", int verbosity = 0) {
		if (collect == "distance") {
			for (int i = 0; i < no_cells; i++) {
				float color = dist_aggregate[i] * 10000;
				color_distribution[i] = color;
			}
		}
		if (collect == "distance_single") {
			for (int i = 0; i < no_cells; i++) {
				float color = d[i] * 10000;
				color_distribution[i] = color;
			}
		}
		else if (collect == "fruits") {
			for (int i = 0; i < no_cells; i++) {
				color_distribution[i] = f[species][i] * 100;
			}
		}
		else if (collect == "cover") {
			for (int i = 0; i < no_cells; i++) {
				color_distribution[i] = c[species][i] * 100;
			}
		}
		else if (collect == "visits") {
			float sum_recipr = 1.0f / visits_sum;
			int max_visits = 0;
			for (int i = 0; i < no_cells; i++) {
				if (visits[i] > max_visits) max_visits = visits[i];
				color_distribution[i] = (float)visits[i] * sum_recipr * 100000;
			}
		}
		else if (collect == "k") {
			for (int i = 0; i < no_cells; i++) {
				float color = selection_probabilities.probabilities[i] * 1000000;
				color_distribution[i] = color;
			}
		}
		if (verbosity > 0) printf("collected %s for species %s \n", collect.c_str(), species.c_str());
		return color_distribution;
	}
	float* get_lookup_table(string species) {
		return dist_lookup_table[species];
	}
	ResourceCell* select_random_cell() {
		int idx = help::get_rand_int(0, size - 1);
		return &cells[idx];
	}
	void precompute_dist_lookup_table(string species) {
		float a_d = animal_kernel_params[species]["a_d"];
		float b_d = animal_kernel_params[species]["b_d"];
		float a_d_recipr = 1.0f / a_d;
		for (int i = 0; i < size; i++) {
			pair<float, float> curpos = get_real_position(cells[i].pos);
			for (int j = 0; j < size; j++) {
				pair<float, float> target_pos = get_real_position(cells[j].pos);
				float dist = get_resourcegrid_dist(curpos, target_pos);
				float val = tanh(pow((-dist * a_d_recipr), b_d));
				dist_lookup_table[species][size * (cells[i].pos.first + cells[i].pos.second * width) + cells[j].pos.first + cells[j].pos.second * width] = val;
			}
			if (i % width == 0) printf("Finished row %i / %i\n", i / width, width);
		}
		printf("Computed dist lookup table for species %s \n", species.c_str());
	}
	void set_dist_lookup_table(float* lookup_table, string species) {
		for (int i = 0; i < width * width * width * width; i++) {
			dist_lookup_table[species][i] = lookup_table[i];
		}
	}
	void compute_d(pair<int, int>& curpos, string species) {
		for (int i = 0; i < size; i++) {
			//pair<float, float> cell_pos = get_real_position(cells[i].pos);
			//float dist = get_resourcegrid_dist(get_real_position(curpos), cell_pos);
			//d[i] = tanh(pow((-dist * 0.5), 0.4));
			//continue;
			
			pair<int, int> target_pos = cells[i].pos;
		    int lookup_idx = size * (curpos.first + curpos.second * width) + target_pos.first + target_pos.second * width;
			/*if (lookup_idx < 0 || lookup_idx > size * size) {
				printf("cur pos: %i, %i, target pos: %i, %i \n", curpos.first, curpos.second, target_pos.first, target_pos.second);
				printf("Idx: %i (limits: %i - %i) \n", lookup_idx, 0, size * size);
			}*/
			d[i] = dist_lookup_table[species][lookup_idx];
		}
	}
	void compute_c(string species, float a_c, float b_c) {
		float* _c = c[species];
		float a_c_recipr = 1.0f / a_c;
		for (int i = 0; i < size; i++) {
			_c[i] = tanh(pow((cover[i] * a_c_recipr), b_c));
		}
	}
	void compute_f(string species, float a_f, float b_f) {
		float* _f = f[species];
		float a_f_recipr = 1.0f / a_f;
		for (int i = 0; i < size; i++) {
			_f[i] = tanh(pow((fruit_abundance[i] * a_f_recipr), b_f));
		}
	}
	void update_cover_and_fruit_probabilities(string species, map<string, float>& species_params) {
		compute_c(species, species_params["a_c"], species_params["b_c"]);
		compute_f(species, species_params["a_f"], species_params["b_f"]);
	}
	void reset_color_arrays() {
		for (int i = 0; i < size; i++) visits[i] = 0;
		for (int i = 0; i < size; i++) dist_aggregate[i] = 0;
		visits_sum = 0;
	}
	ResourceCell* select_cell(string species, pair<float, float> cur_position) {
		pair<int, int> gridbased_curpos = get_gridbased_position(cur_position);
		cap(gridbased_curpos);
		compute_d(gridbased_curpos, species);
		compute_k(species);
		int idx = selection_probabilities.sample();
		visits[idx] += 1;
		visits_sum += 1;
		return &cells[idx];
	}
	State* state = 0;
	Grid* grid = 0;
	ResourceCell* cells = 0;
	map<string, float*> c;
	map<string, float*> f;
	map<string, float*> dist_lookup_table;
	vector<string> species;
	map<string, map<string, float>> animal_kernel_params;	
	float* dist_aggregate = 0;
	float* cover = 0;
	float* fruit_abundance = 0;
	float* d = 0;
	float visits_sum = 0;
	int* visits = 0;
	int* color_distribution = 0;
	int iteration = -1;
	int total_no_fruits = 0;
	int lookup_table_size = 0;
	int size = 0;
	DiscreteProbabilityModel selection_probabilities;
	pair<float, float>* neighbor_offsets = 0;
	bool has_fruits = false;

private:
	void init_cells() {
		int no_gridcells_along_x_per_resource_cell = (float)grid->width / (float)width;
		for (int i = 0; i < size; i++) {
			cells[i] = ResourceCell(idx_2_pos(i), i);
			cells[i].grid_bb_min = no_gridcells_along_x_per_resource_cell * cells[i].pos;
			cells[i].grid_bb_max = cells[i].grid_bb_min + pair<int, int>(no_gridcells_along_x_per_resource_cell, no_gridcells_along_x_per_resource_cell);
		}
	}
	void compute_k(string species) {
		float* _c = c[species];
		float* _f = f[species];
		float sum = 0.0f;
		for (int i = 0; i < size; i++) {
			selection_probabilities.probabilities[i] = d[i] * _c[i] * _f[i];
			sum += selection_probabilities.probabilities[i];
		}
		selection_probabilities.normalize(sum);
		selection_probabilities.build_cdf();
	}
};

