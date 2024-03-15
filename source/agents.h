#pragma once
#include "helpers.h"


class Tree {
public:
	Tree() = default;
	Tree(pair<float, float> _position) : position(_position) {};
	Tree(pair<float, float> _position, float _radius) : position(_position), radius(_radius) {};
	Tree(float _radius, vector<float> _strategy, int _life_phase, pair<float, float> _position):
		radius(_radius), strategy(_strategy), life_phase(_life_phase), position(_position) {};
	float radius = 0;
	vector<float> strategy = {};
	int life_phase = 0;
	pair<float, float> position = pair(0, 0);
};


class Population {
public:
	Population() = default;
	Population(float _mean_radius) : mean_radius(_mean_radius) {}
	Tree* add(pair<float, float> position) {
		Tree tree(position, mean_radius);
		members.push_back(tree);
		return &members.back();
	}
	int size() {
		return members.size();
	}
	vector<Tree> members = {};
	float mean_radius = 0;
};
