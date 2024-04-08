#pragma once
#include "state.h"
#include "grid_agent.forward.h"



class Disperser {
public:
	Disperser() = default;
	Disperser(State* _state) {
		state = _state;
		_pop = &state->population;
		_grid = &state->grid;
	}
	void disperse(Crop* crop) {
		// Compute deposition location
		pair<float, float> direction = get_random_direction();
		float distance = crop->kernel->sample();
		pair<float, float> depos_location = crop->origin + distance * direction;
		Cell* cell = _grid->get_cell_at_position(depos_location);

		// Suppress germination of seeds in areas with increased competition
		if (help::get_rand_float(0, 1) < ((float)cell->trees.size() * state->saturation_threshold)) {
			return;
		}

		// If seed germinates, create a new tree at the seed deposition location 
		Strategy strategy = *(_pop->get_strat(crop->id));
		Tree* tree = _pop->add(depos_location, strategy, 0.1); // TEMP: Arbitrary starting radius of 0.1. TODO: replace with 0 once growth curve is implemented.
		cell->trees[tree->id] = tree->id;
	}
	Population* _pop = 0;
	Grid* _grid = 0;
	State* state = 0;
};


class Anemochory : Disperser {
public:
	Anemochory() = default;
	Anemochory(State* _state) : Disperser(_state) {

	}
};


class Zoochory : Disperser {
public:
	Zoochory() = default;
	Zoochory(State* _state) : Disperser(_state) {

	}
};

