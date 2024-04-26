#pragma once
#include "helpers.h"
#include "grid_agent.forward.h"
#include "kernel.h"


using namespace help;

class Strategy {
public:
	Strategy() = default;
	Strategy(string _vector, float _seed_mass, float _diaspore_mass, int _no_seeds_per_diaspore, float _seed_tspeed, float _pulp_to_seed_ratio) :
		vector(_vector), seed_mass(_seed_mass), diaspore_mass(_diaspore_mass), no_seeds_per_diaspore(_no_seeds_per_diaspore),
		seed_tspeed(_seed_tspeed), pulp_to_seed_ratio(_pulp_to_seed_ratio)
	{}
	void print() {
		printf("id: %d, seed_mass: %f, diaspore_mass: %f, no_seeds_per_diaspore: %d, vector: %s \n",
			id, seed_mass, diaspore_mass, no_seeds_per_diaspore, vector.c_str()
		);
	}
	int id = -1;
	float seed_mass = 0;
	float diaspore_mass = 0;
	int no_seeds_per_diaspore = 0;
	float seed_tspeed = 0;
	float pulp_to_seed_ratio = 0;
	string vector = "none";
};


class StrategyGenerator {
public:
	StrategyGenerator() = default;
	StrategyGenerator(map<string, map<string, float>> user_parameters) {
		for (auto& [trait, value_range] : user_parameters) {
			ProbModel prob_model;
			string distribution_type = distribution_types[value_range["distribution_type"]];
			if (distribution_type == "uniform") {
				prob_model = ProbModel(value_range["min"], value_range["max"]);
			}
			else if (distribution_type == "linear") {
				prob_model = ProbModel(value_range["q1"], value_range["q2"], value_range["min"], value_range["max"]);
			}
			else if (distribution_type == "normal") {
				prob_model = ProbModel(value_range["mean"], value_range["stdev"], 0);
			}
			else if (distribution_type == "discrete") {
				prob_model = ProbModel(value_range["probability0"], value_range["probability1"], value_range["probability2"]);
			}
			trait_distributions[trait] = prob_model;
		}
	}
	string pick_vector() {
		int vector = trait_distributions["vector"].sample();
		if (vector == 0) return "linear";
		else if (vector == 1) return "wind";
		else return "animal";
	}
	float compute_wing_mass(float cumulative_seed_mass) {
		// Estimate wing mass based on correlation we found in seed- and wing measurement data taken from (Greene and Johnson, 1993)
		return max(0, (cumulative_seed_mass - 0.03387f) / 3.6609f);
	}
	float compute_diaspore_mass(float no_seeds_per_diaspore, float seed_mass, string vector, float fruit_pulp_mass, float &pulp_to_seed_ratio) {
		float cumulative_seed_mass = seed_mass * no_seeds_per_diaspore;
		float diaspore_mass;
		if (vector == "wind") {
			float wing_mass = compute_wing_mass(cumulative_seed_mass);
			diaspore_mass = cumulative_seed_mass + wing_mass;
			//printf("cumulative seed mass: %f, wing mass: %f \n", cumulative_seed_mass, wing_mass);
		}
		else if (vector == "animal") {
			diaspore_mass = cumulative_seed_mass + fruit_pulp_mass;
		}
		else {
			diaspore_mass = cumulative_seed_mass;
		}
		return diaspore_mass;
	}
	float calculate_tspeed(float diaspore_mass) {
		// Compute terminal descent velocity based on correlation presented by (Greene and Johnson, 1993)
		return 0.501f * pow(diaspore_mass * 1000, 0.174f);
	}
	int sample_no_seeds_per_diaspore() {
		int no_seeds_per_diaspore = round(trait_distributions["no_seeds_per_diaspore"].sample());
		if (no_seeds_per_diaspore < 1) no_seeds_per_diaspore = 1; // Ensure at least one seed per diaspore
		return no_seeds_per_diaspore;
	}
	float sample_seed_mass(string vector) {
		float seed_mass = trait_distributions["seed_mass"].sample();
		if (vector == "wind") seed_mass = min(2.0f, seed_mass); // Cap seed mass for wind dispersal to ~largest observed
		return seed_mass;
	}
	float sample_fruit_pulp_mass() {
		return trait_distributions["fruit_pulp_mass"].sample();
	}
	void generate(Strategy &strategy) {
		int no_seeds_per_diaspore = sample_no_seeds_per_diaspore();
		string vector = pick_vector();
		float seed_mass = sample_seed_mass(vector);
		float fruit_pulp_mass = sample_fruit_pulp_mass();
		float pulp_to_seed_ratio;
		float diaspore_mass = compute_diaspore_mass(no_seeds_per_diaspore, seed_mass, vector, fruit_pulp_mass, pulp_to_seed_ratio);
		float seed_tspeed = calculate_tspeed(diaspore_mass);
		strategy = Strategy(vector, seed_mass, diaspore_mass, no_seeds_per_diaspore, seed_tspeed, pulp_to_seed_ratio);
	}
	void mutate(Strategy& strategy, float mutation_rate) {
		bool do_mutation = help::get_rand_float(0, 1) < mutation_rate;
		if (do_mutation) {
			printf("\nstrategy before mutation: \n");
			strategy.print();
			int trait_idx = help::get_rand_int(0, 3);
			float fruit_pulp_mass = 0;
			if (trait_idx == 0) {
				strategy.seed_mass = sample_seed_mass(strategy.vector);
			}
			else if (trait_idx == 1) {
				strategy.no_seeds_per_diaspore = sample_no_seeds_per_diaspore();
			}
			else if (trait_idx == 2) {
				fruit_pulp_mass = sample_fruit_pulp_mass();
			}
			else if (trait_idx == 3) {
				strategy.vector = pick_vector();
			}
			float pulp_to_seed_ratio;
			strategy.diaspore_mass = compute_diaspore_mass(strategy.no_seeds_per_diaspore, strategy.seed_mass, strategy.vector, fruit_pulp_mass, pulp_to_seed_ratio);
			strategy.seed_tspeed = calculate_tspeed(strategy.diaspore_mass);
			strategy.pulp_to_seed_ratio = pulp_to_seed_ratio;
			printf("strategy after mutation: \n");
			strategy.print();
		}
	}
	map<string, ProbModel> trait_distributions;
	map<int, string> distribution_types = { { 0, "uniform" }, {1, "linear"}, {2, "normal"}, {3, "discrete"} };
};


class Tree {
public:
	Tree() = default;
	Tree(int _id, pair<float, float> _position, float _radius, float _radius_tmin1) : position(_position), radius(_radius), radius_tmin1(_radius_tmin1) {
		id = _id;
	};
	Tree(int _id, pair<float, float> _position, float _radius, float _radius_tmin1, int _life_phase) :
		position(_position), radius(_radius), radius_tmin1(_radius_tmin1), life_phase(_life_phase)
	{
		id = _id;
	};
	bool operator==(const Tree& tree) const
	{
		return id == tree.id;
	}
	bool is_within_radius(pair<float, float> pos2) {
		float dist = help::get_dist(position, pos2);
		return dist < radius;
	}
	void update_life_phase(float max_radius, float seed_bearing_threshold) {
		if (radius > (seed_bearing_threshold * max_radius)) life_phase = 2;
	}
	float radius = -1;
	float radius_tmin1 = -1;
	int life_phase = 0;
	int last_mortality_check = 0;
	pair<float, float> position = pair(0, 0);
	int id = -1;
};


class Crop {
public:
	Crop() = default;
	Crop(Strategy &_strategy, Tree& tree, float _mass_budget_factor) {
		strategy = _strategy;
		seed_mass = _strategy.seed_mass;
		mass_budget_factor = _mass_budget_factor;
		origin = tree.position;
		id = tree.id;
	}
	void compute_mass(Tree& tree) {
		float radius_avg = (tree.radius + tree.radius_tmin1) * 0.5;
		//printf("radius avg: %f, cur radius: %f, old radius: %f, cubed radius: %f \n", radius_avg, tree_radius, tree_radius_tmin1, cubed(radius_avg));
		mass = cubed(radius_avg) - (cubed(tree.radius) - cubed(tree.radius_tmin1));
		if (mass < 0) {
			printf("mass calculated: %f, \n", mass);
			printf("tree id: %i, crop id %i, radius: %f, radius_tmin1: %f \n", tree.id, id, tree.radius, tree.radius_tmin1);
		}
		mass = max(0, mass);
		mass *= mass_budget_factor * 1000;
	}
	void compute_no_seeds() {
		no_seeds = no_diaspora * strategy.no_seeds_per_diaspore;
	}
	void compute_no_diaspora() {
		no_diaspora = mass / strategy.diaspore_mass;
	}
	void update(Tree& tree) {
		if (tree.id != id) printf("\n\n--------------- CROP ID (%i) DOES NOT MATCH TREE ID (%i) ---------------\n\n", id, tree.id);
		compute_mass(tree);
		compute_no_diaspora();
		compute_no_seeds();
	}
	float mass = 0;
	int no_seeds = 0;
	int no_diaspora = 0;
	float seed_mass = 0;
	float mass_budget_factor = 0;
	Strategy strategy;
	pair<float, float> origin = pair<float, float>(0, 0);
	int id = -1;
};


class Population {
public:
	Population() = default;
	Population(float _max_radius, float _cellsize, float _radius_q1, float _radius_q2, float _mass_budget_factor, 
		map<string, map<string, float>> strategy_parameters, float _mutation_rate
	) : max_radius(_max_radius), cellsize(_cellsize), radius_q1(_radius_q1), radius_q2(_radius_q2),
		mass_budget_factor(_mass_budget_factor)
	{
		strategy_generator = StrategyGenerator(strategy_parameters);
		radius_probability_model = help::LinearProbabilityModel(radius_q1, radius_q2, 0, max_radius);
		mutation_rate = _mutation_rate;
	}
	Tree* add(pair<float, float> position, Strategy* _strategy = 0, float radius = -2) {
		// Create tree
		if (radius == -1) radius = max_radius;
		else if (radius == -2) radius = radius_probability_model.linear_sample();
		float radius_tmin1 = radius * 0.9; // TEMP. TODO: Make dependent on growth curve.
		Tree tree(no_created_trees + 1, position, radius, radius_tmin1, 1);
		members[tree.id] = tree;
		no_created_trees++;

		// Create strategy
		Strategy strategy;
		if (_strategy != nullptr) {
			strategy = *_strategy;
			strategy_generator.mutate(strategy, mutation_rate);
		}
		else {
			strategy_generator.generate(strategy);
		} 
		strategy.id = tree.id;
		strategies[tree.id] = strategy;

		// Create custom kernel
		Kernel kernel = kernels[strategy.vector];
		if (strategy.vector == "wind") {
			kernel = Kernel(kernel.dist_max, kernel.wspeed_gmean, kernel.wspeed_stdev, kernel.wind_direction, kernel.wind_direction_stdev, strategy.seed_tspeed, max_radius * 4);
		}
		kernels_individual[tree.id] = kernel;

		// Create crop
		Crop crop(strategy, tree, mass_budget_factor);
		crops[tree.id] = crop;

		return &members[tree.id];
	}
	Kernel* add_kernel(string tree_dispersal_vector, Kernel &kernel) {
		kernels[tree_dispersal_vector] = kernel;
		return &kernel;
	}
	Tree* get(int id) {
		return &members[id];
	}
	void get(vector<int>& ids, vector<Tree*> &trees) {
		for (int id : ids) trees.push_back(get(id));
	}
	Crop* get_crop(int id) {
		return &crops[id];
	}
	Strategy* get_strat(int id) {
		return &strategies[id];
	}
	Kernel* get_kernel(string vector) {
		return &kernels[vector];
	}
	Kernel* get_kernel(int id) {
		return &kernels_individual[id];
	}
	int size() {
		return members.size();
	}
	void remove(Tree* tree) {
		remove(tree->id);
	}
	void remove(int id) {
		members.erase(id);
		crops.erase(id);
		strategies.erase(id);
		kernels_individual.erase(id);
	}
	bool is_population_member(Tree* tree) {
		auto it = members.find(tree->id);
		return (it != members.end());
	}
	bool is_population_member(int tree_id) {
		return members.find(tree_id) != members.end();
	}
	unordered_map<int, Tree> members;
	unordered_map<int, Crop> crops;
	unordered_map<int, Strategy> strategies;
	unordered_map<string, Kernel> kernels;
	unordered_map<int, Kernel> kernels_individual;
	help::LinearProbabilityModel radius_probability_model;
	StrategyGenerator strategy_generator;
	float max_radius = 0;
	float cellsize = 0;
	float radius_q1 = 0;
	float radius_q2 = 0;
	float seed_mass = 0;
	float mutation_rate = 0;
	Tree removed_tree;
	int no_created_trees = 0;
	float mass_budget_factor = 0;
};
