#pragma once
#include "agents.h"
#include "grid_agent.forward.h"

class Cell {
public:
	Cell() = default;
	int state = 0;
	int idx = 0;
	float time_last_fire = 0;
	map<int, int> trees;
	float cumulative_radius = 0;
	pair<int, int> pos;
	bool operator==(const Cell& cell) const
	{
		return pos == cell.pos;
	}
};


class Grid {
public:
	Grid() = default;
	Grid(int _width, float _cellsize) {
		width = _width;
		cellsize = _cellsize;
		width_r = (float)width * cellsize;
		no_cells = width * width;
		no_savanna_cells = no_cells;
		init_grid_cells();
		reset_state_distr();
		area = no_cells * cellsize * cellsize;
	}
	void init_grid_cells() {
		distribution = new Cell[no_cells];
		for (int i = 0; i < no_cells; i++) {
			pair<int, int> pos = idx_2_pos(i);
			distribution[i].pos = pos;
			distribution[i].idx = i;
		}
		state_distribution = new int[no_cells];
	}
	int pos_2_idx(pair<int, int> pos) {
		return width * pos.second + pos.first;
	}
	pair<float, float> get_random_real_position() {
		float x = help::get_rand_float(0, width_r);
		float y = help::get_rand_float(0, width_r);
		pair<float, float> position = pair(x, y);
		return position;
	}
	pair<int, int> get_random_grid_position() {
		int x = help::get_rand_uint(0, width);
		int y = help::get_rand_uint(0, width);
		pair<int, int> position(x, y);
		return position;
	}
	Cell* get_random_savanna_cell() {
		int i = 0;
		int fetch_attempt_limit = 1e6;
		while (i < 1e6) {
			pair<int, int> pos = get_random_grid_position();
			Cell* cell = get_cell_at_position(pos);
			if (cell->state == 0) return cell;
		}
		throw("Runtime error: Could not find savanna cell after %i attempts.\n", fetch_attempt_limit);
	}
	void reset() {
		for (int i = 0; i < no_cells; i++) {
			distribution[i].state = 0;
			distribution[i].trees.clear();
			distribution[i].cumulative_radius = 0;
		}
		no_forest_cells = 0;
		no_savanna_cells = no_cells;
	}
	void reset_state_distr() {
		for (int i = 0; i < no_cells; i++) {
			state_distribution[i] = 0;
		}
	}
	void redo_count() {
		no_savanna_cells = 0;
		no_forest_cells = 0;
		for (int i = 0; i < no_cells; i++) {
			no_savanna_cells += !distribution[i].state;
			no_forest_cells += distribution[i].state;
		}
	}
	pair<int, int> idx_2_pos(int idx) {
		int x = idx % width;
		int y = idx / width;
		return pair<int, int>(x, y);
	}
	float get_tree_cover() {
		return (float)no_forest_cells / (float)(no_cells);
	}
	Cell* get_cell_at_position(pair<int, int> pos) {
		cap(pos);
		return &distribution[pos.second * width + pos.first];
	}
	Cell* get_cell_at_position(pair<float, float> _pos) {
		pair<int, int> pos = get_gridbased_position(_pos);
		return get_cell_at_position(pos);
	}
	void get_cells_within_radius(Tree* tree, vector<Cell*>* cells, bool remove_cells = false) {
		pair<float, float> tree_center_gb = get_gridbased_position(tree->position);
		int radius_gb = tree->radius / cellsize;
		for (int x = tree_center_gb.first - radius_gb; x <= tree_center_gb.first + radius_gb; x++) {
			for (int y = tree_center_gb.second - radius_gb; y <= tree_center_gb.second + radius_gb; y++) {
				if (help::get_dist(pair<float, float>(x, y), tree_center_gb) < radius_gb) {
					Cell* cell = get_cell_at_position(pair<int, int>(x, y));
					if (remove_cells) {
						auto it = find(cells->begin(), cells->end(), cell);
						if (it != cells->end()) cells->erase(it);
					}
					else cells->push_back(cell);
				}
			}
		}
	}
	void populate_tree_domain(Tree* tree) {
		pair<float, float> tree_center_gb = get_gridbased_position(tree->position);
		int radius_gb = tree->radius / cellsize;
		for (float x = tree_center_gb.first - radius_gb; x <= tree_center_gb.first + radius_gb; x+=1) {
			for (float y = tree_center_gb.second - radius_gb; y <= tree_center_gb.second + radius_gb; y+=1) {
				pair<float, float> position(x * cellsize, y * cellsize);
				if (tree->is_within_radius(position)) {
					set_to_forest(pair<float, float>(x, y), tree);
				}
			}
		}
	}
	void burn_tree_domain(Tree* tree, queue<Cell*>& queue, float time_last_fire = -1) {
		pair<float, float> tree_center_gb = get_gridbased_position(tree->position);
		int radius_gb = round((tree->radius * 1.5) / cellsize);
		for (float x = tree_center_gb.first - radius_gb; x <= tree_center_gb.first + radius_gb; x += 1) {
			for (float y = tree_center_gb.second - radius_gb; y <= tree_center_gb.second + radius_gb; y += 1) {
				pair<float, float> position(x * cellsize, y * cellsize);
				if (tree->is_within_radius(position)) {
					Cell* cell = get_cell_at_position(position);
					queue.push(cell);
					if (distribution[cell->idx].state == 0) continue;
					
					// Remove tree id from cell->trees.
					int map_size = cell->trees.size();
					auto it = cell->trees.find(tree->id);
					if (it != cell->trees.end()) cell->trees.erase(it);

					// Update cumulative radius
					cell->cumulative_radius -= tree->radius;

					// Set cell to savanna if it is no longer occupied by trees larger than the cell itself.
					if (cell->cumulative_radius < (cellsize * 0.7)) {
						set_to_savanna(cell->idx, time_last_fire);
						state_distribution[cell->idx] = -6;
						continue;
					}
					state_distribution[cell->idx] = -5;
				}
			}
		}
	}
	int* get_state_distribution(bool collect = true) {
		if (collect) {
			for (int i = 0; i < no_cells; i++) {
				if (distribution[i].state == 1) state_distribution[i] = distribution[i].trees.begin()->first % 100 + 1;
			}
		}
		return state_distribution;
	}
	void set_state_distribution(int* distr) {
		for (int i = 0; i < no_cells; i++) {
			if (distr[i] >= 0 && distr[i] < 10)
				state_distribution[i] = distr[i];
			else {
				state_distribution[i] = 0;
			}
		}
	}
	void set_to_forest(int idx, Tree* tree) {
		if (distribution[idx].state == 0) {
			no_savanna_cells--;
			no_forest_cells++;
		}
		distribution[idx].state = 1;
		distribution[idx].trees[tree->id] = tree->id;
		if (tree->id == 0) cout << "pushing back tree id 0\n";
		distribution[idx].cumulative_radius += tree->radius;
		distribution[idx].time_last_fire = 0;
	}
	void set_to_savanna(int idx, float _time_last_fire = -1) {
		no_savanna_cells += (distribution[idx].state == 1);
		no_forest_cells -= (distribution[idx].state == 1);

		distribution[idx].state = 0;
		distribution[idx].time_last_fire = _time_last_fire;
	}
	void set_to_forest(pair<int, int> position_grid, Tree* tree) {
		cap(position_grid);
		set_to_forest(position_grid.second * width + position_grid.first, tree);
	}
	void set_to_savanna(pair<int, int> position_grid, float time_last_fire = -1) {
		cap(position_grid);
		set_to_savanna(pos_2_idx(position_grid), time_last_fire);
	}
	void cap(pair<int, int> &position_grid) {
		if (position_grid.first < 0) position_grid.first = width + (position_grid.first % width);
		if (position_grid.second < 0) position_grid.second = width + (position_grid.second % width);
		position_grid.first %= width;
		position_grid.second %= width;
	}
	pair<int, int> get_gridbased_position(pair<float, float> position) {
		return pair<int, int>(position.first / cellsize, position.second / cellsize);
	}
	pair<float, float> get_real_position(pair<int, int> position) {
		return pair<int, int>(position.first * cellsize, position.second * cellsize);
	}
	int width = 0;
	int no_cells = 0;
	float width_r = 0;
	float cellsize = 1.5;
	Cell* distribution = 0;
	int* state_distribution = 0;
	int no_savanna_cells = 0;
	int no_forest_cells = 0;
	float area = 0;
};
