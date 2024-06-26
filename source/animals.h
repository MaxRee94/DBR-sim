#pragma once
#include "animal_resources.h"
#include "grid_agent.forward.h"


class Animal {
public:
	Animal() = default;
	Animal(map<string, float> _traits, string _species) {
		traits = _traits;
		traits["a_c_recipr"] = 1.0f / traits["a_c"];
		traits["a_f_recipr"] = 1.0f / traits["a_f"];
		species = _species;
		recipr_speed = 1.0f / traits["speed"];
		init_prob_distributions();
	}
	void init_prob_distributions() {
		gut_passage_time_distribution = GammaProbModel(traits["gut_passage_time_shape"], traits["gut_passage_time_scale"]);
		rest_time_distribution = GammaProbModel(traits["rest_time_shape"], traits["rest_time_scale"]);
	}
	void reset() {
		curtime = 0;
		stomach_content.clear();
		total_no_seeds_consumed = 0;
	}
	void update(
		int& no_seeds_dispersed, int _iteration, State* state, ResourceGrid* resource_grid, int& no_seeds_eaten,
		float& time_spent_resting, float& distance_travelled, float& average_gut_passage_time, float& average_gut_time,
		int& no_seeds_defecated, vector<int>& compute_times
	) {
		float begin_time = curtime;
		iteration = _iteration;

		auto start = high_resolution_clock::now();
		pair<int, int> move_time_interval(curtime, curtime);
		move(resource_grid, distance_travelled);
		compute_times[0] += help::microseconds_elapsed_since(start);
		move_time_interval.second = curtime;

		start = high_resolution_clock::now();
		digest(resource_grid, no_seeds_dispersed, no_seeds_defecated, compute_times, state, move_time_interval);
		compute_times[1] += help::microseconds_elapsed_since(start);

		start = high_resolution_clock::now();
		pair<int, int> rest_time_interval(curtime, curtime);
		rest(time_spent_resting);
		compute_times[2] += help::microseconds_elapsed_since(start);
		rest_time_interval.second = curtime;

		eat(resource_grid, begin_time, no_seeds_eaten, compute_times);

		start = high_resolution_clock::now();
		digest(resource_grid, no_seeds_dispersed, no_seeds_defecated, compute_times, state, rest_time_interval);
		compute_times[1] += help::microseconds_elapsed_since(start);

		//// TEMPORARY: Calculate the average gut passage time.
		//average_gut_passage_time = 0;
		//for (auto& [seed_id, seed_plus_times] : stomach_content) {
		//	auto [gut_passage_time, ingestion_time] = seed_plus_times.second;
		//	average_gut_passage_time += gut_passage_time;
		//}
		//average_gut_passage_time /= (float)stomach_content.size();

		//// TEMPORARY: Calculate the average time each seed currently in the stomach has been there.
		//average_gut_time = 0;
		//for (auto& [seed_id, seed_plus_times] : stomach_content) {
		//	auto [gut_passage_time, ingestion_time] = seed_plus_times.second;
		//	average_gut_time += curtime - ingestion_time;
		//}
		//average_gut_time /= (float)stomach_content.size();
	}
	void rest(float& time_spent_resting) {
		moving = false;
		float rest_time = rest_time_distribution.get_gamma_sample() * 60.0f; // Multiply by 60 to convert minutes to seconds.
		curtime += rest_time;
		time_spent_resting += rest_time;
	}
	float get_biomass_appetite() {
		return 1000; // We assume that the group of animals will eat 1 kg of fruit in a single session.
	}
	void eat(ResourceGrid* resource_grid, float begin_time, int& no_seeds_eaten, vector<int>& compute_times) {
		auto start = high_resolution_clock::now();

		float biomass_appetite = get_biomass_appetite();
		vector<pair<float, int>> consumed_seed_gpts_plus_cropids = {};
		while (biomass_appetite > 0) {
			Fruit fruit;

			auto _start1 = high_resolution_clock::now();
			bool fruit_available = resource_grid->extract_fruit(position, fruit, compute_times);
			compute_times[6] += help::microseconds_elapsed_since(_start1);

			if (!fruit_available) break;

			auto _start2 = high_resolution_clock::now();
			eat_fruit(fruit, consumed_seed_gpts_plus_cropids);
			compute_times[7] += help::microseconds_elapsed_since(_start2);

			no_seeds_eaten += fruit.strategy.no_seeds_per_diaspore;
			biomass_appetite -= fruit.strategy.diaspore_mass;
		}
		auto _start3 = high_resolution_clock::now();
		set_defecation_times(begin_time, consumed_seed_gpts_plus_cropids);
		compute_times[18] += help::microseconds_elapsed_since(_start3);

		compute_times[3] += help::microseconds_elapsed_since(start);
	}
	void set_defecation_times(float begin_time, vector<pair<float, int>>& consumed_seed_gpts_plus_cropids) {
		float time_stepsize = (curtime - begin_time) / (float)consumed_seed_gpts_plus_cropids.size();
		for (int i = consumed_seed_gpts_plus_cropids.size() - 1; i >= 0; i--) {
			float ingestion_time = curtime - (float)(i + 1) * time_stepsize;
			float gut_passage_time = consumed_seed_gpts_plus_cropids[i].first;
			int crop_id = consumed_seed_gpts_plus_cropids[i].second;
			int defecation_time = round(gut_passage_time + ingestion_time);
			if (stomach_content[defecation_time].size() == 0) stomach_content[defecation_time] = { crop_id };
			else stomach_content[defecation_time].push_back(crop_id);
		}
	}
	void eat_fruit(Fruit &fruit, vector<pair<float, int>>& consumed_seed_gpts_plus_cropids) {
		vector<Seed> seeds;
		//fruit.get_seeds(seeds);
		for (int i = 0; i < fruit.strategy.no_seeds_per_diaspore; i++) {
			float gut_passage_time = gut_passage_time_distribution.get_gamma_sample();
			
			// Multiply GPT by 60 to convert minutes to seconds.
			gut_passage_time *= 60.0f;

			consumed_seed_gpts_plus_cropids.push_back(pair<float, int>(gut_passage_time, fruit.id));
			total_no_seeds_consumed++;
		}
	}
	void digest(
		ResourceGrid* resource_grid, int& no_seeds_dispersed, int& no_seeds_defecated, vector<int>& compute_times,
		State* state
	) {
		vector<int> deletion_schedule;
		auto start = high_resolution_clock::now();
		for (auto& [seed_id, deftime_plus_crop_id] : stomach_content) {
			auto _start = high_resolution_clock::now();
			
			float defecation_time = deftime_plus_crop_id.first;
			if (defecation_time <= curtime) {
				deletion_schedule.push_back(seed_id);
				if (iteration < 5) continue; // Do not disperse in the first 5 iterations (after Morales et al 2013)

				auto __start = high_resolution_clock::now();
				float time_since_defecation = curtime - defecation_time;
				Seed to_defecate(state->population.get_crop(deftime_plus_crop_id.second)->strategy);
				compute_times[17] += help::microseconds_elapsed_since(__start);

				__start = high_resolution_clock::now();
				defecate(to_defecate, time_since_defecation, no_seeds_dispersed, resource_grid);
				compute_times[13] += help::microseconds_elapsed_since(__start);

				no_seeds_defecated++;
			}

			compute_times[15] += help::microseconds_elapsed_since(_start);
		}                  
		compute_times[16] += help::microseconds_elapsed_since(start);

		start = high_resolution_clock::now();
		int no_seeds_erased = 0;
		for (auto& seed_id : deletion_schedule) {
			no_seeds_erased++;
			stomach_content.erase(seed_id);
		}
		compute_times[14] += help::microseconds_elapsed_since(start);
		//printf("No seeds defecated %i, no seeds erased %i\n", no_seeds_defecated, no_seeds_erased);
	}
	pair<float, float> select_destination(ResourceGrid* resource_grid) {
		ResourceCell* cell = resource_grid->select_cell(species, position);
		pair<float, float> destination;
		//ResourceCell* cell = resource_grid->get_random_resource_cell();
		resource_grid->get_random_location_within_cell(cell, destination);
		return destination;
	}
	void move(ResourceGrid* resource_grid, float& distance_travelled) {
		moving = true;
		pair<float, float> destination = select_destination(resource_grid);
		float distance;
		trajectory = resource_grid->get_shortest_trajectory(position, destination, distance);
		position = destination;
		distance_travelled += distance;
		travel_time = distance * recipr_speed;
		curtime += travel_time;
	}
	pair<float, float> get_backwards_traced_location(float time_since_defecation) {
		return position - (time_since_defecation / travel_time) * trajectory;
	}
	void defecate(Seed &seed, float time_since_defecation, int &no_seeds_dispersed, ResourceGrid* resource_grid) {
		pair<float, float> seed_deposition_location;
		if (moving) {
			seed_deposition_location = get_backwards_traced_location(time_since_defecation);
		}
		else {
			seed_deposition_location = position;
		}

		int prev_seed_dispersed_number = no_seeds_dispersed;
		for (int i = 0; i < 2; i++) {
			resource_grid->get_random_stategrid_location(seed_deposition_location);
			seed.deposition_location = seed_deposition_location;
			bool germination = seed.germinate_if_location_is_viable(resource_grid->state);
			no_seeds_dispersed += germination;
		}
		//printf("No seeds dispersed: %i \n", no_seeds_dispersed);
	}
	map<string, float> traits;
	unordered_map<int, vector<int>> stomach_content; // { defecation_time: crop_ids }
	pair<float, float> position;
	pair<float, float> trajectory;
	string species;
	GammaProbModel gut_passage_time_distribution;
	GammaProbModel rest_time_distribution;
	float curtime = 0;
	float recipr_speed = 0;
	float travel_time = 0;
	int iteration = -1;
	int verbosity = 0;
	int total_no_seeds_consumed = 0;
	bool moving = false;
};


class Animals {
public:
	Animals() = default;
	Animals(State* state, map<string, map<string, float>> _animal_kernel_params) {
		animal_kernel_params = _animal_kernel_params;
		total_no_animals = round(animal_kernel_params["population"]["density"] * (state->grid.area / 1e6));
		no_iterations = animal_kernel_params["population"]["no_iterations"];
		animal_kernel_params.erase("population");
	}
	void print_animal_kernel_params(map<string, map<string, float>>& _animal_kernel_params) {
		for (auto& [species, params] : animal_kernel_params) {
			printf("species: %s\n", species.c_str());
			for (auto& [param, value] : params) {
				printf("%s: %f\n", param.c_str(), value);
			}
		}
	}
	void initialize_population() {
		for (auto& [species, params] : animal_kernel_params) {
			int popsize = round(total_no_animals * params["population_fraction"]);
			vector<Animal> species_population;
			create_species_population(popsize, species_population, params, species);
			total_animal_population[species] = species_population;
		}
	}
	void create_species_population(
		int popsize, vector<Animal>& species_population, map<string, float> traits, string species
	) {
		for (int i = 0; i < popsize; i++) {
			Animal animal(traits, species);
			species_population.push_back(animal);
		}
	}
	void place(State* state) {
		for (auto& [species, species_population] : total_animal_population) {
			for (auto& animal : species_population) {
				animal.reset();
				animal.position = state->grid.get_random_real_position();
			}
		}
	}
	void disperse(int& no_seeds_dispersed, int no_seeds_to_disperse, State* state, ResourceGrid* resource_grid) {
		place(state);
		resource_grid->reset_color_arrays();
		int iteration = 0;

		int no_seeds_eaten = 0;
		int no_seeds_defecated = 0;
		vector<int> compute_times = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		auto overall_start_time = high_resolution_clock::now();
		while (no_seeds_defecated < no_seeds_to_disperse) {
			int prev_no_seeds_dispersed = no_seeds_dispersed;
			int prev_no_seeds_defecated = no_seeds_defecated;
			int cur_no_seeds_eaten = 0;
			int cur_no_seeds_defecated = 0;
			float time_spent_resting = 0;
			float distance_travelled = 0;
			float average_gut_passage_time = 0;
			float average_curtime = 0;
			float average_gut_time = 0;
			for (auto& [species, species_population] : total_animal_population) {
				resource_grid->update_cover_and_fruit_probabilities(species, species_population[0].traits);
				for (auto& animal : species_population) {
					auto start_time = high_resolution_clock::now();
					compute_times[5] += help::microseconds_elapsed_since(start_time);
					float _average_gut_time = 0;
					float _average_gut_passage_time = 0;
					start_time = high_resolution_clock::now();
					animal.update(
						no_seeds_dispersed, iteration, state, resource_grid, cur_no_seeds_eaten, time_spent_resting,
						distance_travelled, _average_gut_passage_time, _average_gut_time, no_seeds_defecated, compute_times
					);
					compute_times[4] += help::microseconds_elapsed_since(start_time);
					average_curtime += animal.curtime;
					average_gut_time += _average_gut_time;
					average_gut_passage_time += _average_gut_passage_time;
				}
			}
			no_seeds_eaten += cur_no_seeds_eaten;
			if (iteration % 10 == 0) {
				printf(
					"Number of seeds dispersed in current iteration: %i, number of seeds eaten: %i, number of seeds defecated: %i\n",
					no_seeds_dispersed - prev_no_seeds_dispersed, cur_no_seeds_eaten, no_seeds_defecated - prev_no_seeds_defecated
				);
				printf("Time spent resting: %f minutes, distance travelled: %f meters\n", time_spent_resting, distance_travelled);
				printf("Number of seeds dispersed (total): %s, number of seeds eaten (total): %s\n", help::readable_number(no_seeds_dispersed).c_str(), help::readable_number(no_seeds_eaten).c_str());
				printf("Average gut passage time: %f \n", average_gut_passage_time / (float)popsize());
				printf("Average curtime: %f\n", average_curtime / (float)popsize());
				printf("Average gut time: %f\n", average_gut_time / (float)popsize());
				printf("Current total number of seeds in animal stomachs: %s\n\n", help::readable_number(no_seeds_eaten - no_seeds_defecated).c_str());
				printf("- Total movement times: %s\n", help::readable_number(compute_times[0]).c_str());
				printf("- Total digestion times: %s\n", help::readable_number(compute_times[1]).c_str());
				printf("- Total rest times: %s\n", help::readable_number(compute_times[2]).c_str());
				printf("- Total eat times: %s\n", help::readable_number(compute_times[3]).c_str());
				printf("- Total update times: %s\n", help::readable_number(compute_times[4]).c_str());
				printf("- Total cover and fruit update times: %s\n", help::readable_number(compute_times[5]).c_str());
				printf("- Total fruit extraction times: %s\n", help::readable_number(compute_times[6]).c_str());
				printf("- Total fruit eating times: %s\n", help::readable_number(compute_times[7]).c_str());
				printf("- Total resource cell selection times: %s\n", help::readable_number(compute_times[8]).c_str());
				printf("- Total fruit type selection times: %s\n", help::readable_number(compute_times[9]).c_str());
				printf("- Total fruit in-diaspora fruit extraction times: %s\n", help::readable_number(compute_times[10]).c_str());
				printf("- Total size-check times: %s\n", help::readable_number(compute_times[12]).c_str());
				printf("- Total defecation times: %s\n", help::readable_number(compute_times[13]).c_str());
				printf("- Total seed erasure times: %s\n", help::readable_number(compute_times[14]).c_str());
				printf("- Total seed iteration times: %s\n", help::readable_number(compute_times[15]).c_str());
				printf("- Total seed creation times: %s\n", help::readable_number(compute_times[17]).c_str());
				printf("- Total for-loop seed iteration times: %s\n", help::readable_number(compute_times[16]).c_str());
				printf("- Overall time elapsed: %s\n", help::readable_number(help::microseconds_elapsed_since(overall_start_time)).c_str());
			}
			iteration++;
		}
		printf("-- Number of iterations spent dispersing fruits: %d\n", iteration);
	}
	int popsize() {
		int total_popsize = 0;
		for (auto& [species, species_population] : total_animal_population) {
			total_popsize += species_population.size();
		}
		return total_popsize;
	}
	map<string, vector<Animal>> total_animal_population;
	map<string, map<string, float>> animal_kernel_params;
	float total_no_animals = 0;
	int verbose = 0;
	int no_iterations = 0;
};
