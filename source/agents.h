#pragma once
#include "helpers.h"


class Tree {
public:
	Tree() = default;
	Tree(pair<float, float> _position) : position(_position) {};
	Tree(pair<float, float> _position, float _radius) : position(_position), radius(_radius) {
		id = help::get_rand_uint(0, 10e22);
	};
	Tree(float _radius, vector<float> _strategy, int _life_phase, pair<float, float> _position):
		radius(_radius), strategy(_strategy), life_phase(_life_phase), position(_position)
	{
		id = help::get_rand_uint(0, 10e22);
	};
	bool operator==(const Tree& tree) const
	{
		return id == tree.id;
	}
	void grow(float max_radius) {
		// TEMP: constant growth rate. TODO: Make dependent on radius and life phase (resprout or seedling)
		radius = min(radius + 0.05 * max_radius, max_radius);
	}
	bool is_within_radius(pair<float, float> pos2) {
		float dist = help::get_dist(position, pos2);
		return dist < radius;
	}
	float radius = 0;
	vector<float> strategy = {};
	int life_phase = 0;
	pair<float, float> position = pair(0, 0);
	int id = 0;
};


class Population {
public:
	Population() = default;
	Population(float _max_radius, float _cellsize, float _radius_q1, float _radius_q2) : max_radius(_max_radius),
		cellsize(_cellsize), radius_q1(_radius_q1), radius_q2(_radius_q2) 
	{
		//help::init_RNG();
	}
	Tree* add(pair<float, float> position, string radius="sampled") {
		float _radius = max_radius;
		if (radius == "sampled") _radius = help::sample_linear_distribution(radius_q1, radius_q2, 0, max_radius);
		Tree tree(position, _radius);
		members.push_back(tree);
		return &members.back();
	}
	int size() {
		return members.size();
	}
	void grow() {
		for (auto& tree : members) {
			tree.grow(max_radius);
		}
	}
	vector<Tree*> get_neighbors(Tree* base) {
		vector<Tree*> neighbors;
		float dist = (round(base->radius * 1.1) + max_radius);
		for (auto& candidate : members) {
			if (candidate == *base) continue;
			if (help::get_dist(candidate.position, base->position) < dist) {
				neighbors.push_back(&candidate);
			}
		}
		return neighbors;
	}
	void remove(Tree* tree) {
		auto it = std::find(members.begin(), members.end(), *tree);
		if (it != members.end()) { members.erase(it); }
		else printf("------ ERROR: Population member not found.\n");
	}
	bool is_population_member(Tree* tree) {
		auto it = std::find(members.begin(), members.end(), *tree);
		return (it != members.end());
	}
	vector<Tree> members = {};
	float max_radius = 0;
	float cellsize = 0;
	float radius_q1 = 0;
	float radius_q2 = 0;
};
