#pragma once
#include <vector>
#include <map>
#include <cstdlib>
#include <set>
#include <Windows.h>
#include <string>
#include <iostream>
#include <queue>
#include <unordered_map>

#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>



using namespace std;
#define INV_RAND_MAX  1.0f / (float)RAND_MAX


typedef unsigned int uint;

// Comparison function for sorting the
// set by increasing order of its pair's
// second value
struct _comparator {
	template <typename T>

	// Comparator function
	bool operator()(const T& l, const T& r) const
	{
		if (l.second != r.second) {
			return l.second < r.second;
		}
		return l.first < r.first;
	}
};

typedef std::set < std::pair<int, double>, _comparator> PairSet;

template <typename T, typename U>
std::pair<T, U> operator+(const std::pair<T, U>& l, const std::pair<T, U>& r) {
	return { l.first + r.first,l.second + r.second };
}

template <typename T, typename U>
std::pair<T, U> operator*(const float& s, const std::pair<T, U>& p) {
	return { s * p.first, s * p.second };
}

template <typename T, typename U>
std::pair<T, U> operator*(const std::pair<T, U>& p, const float& s) {
	return s * p;
} 

namespace help {

	void init_RNG();

	void sort(std::map<int, double>& _map, PairSet& _set);

	int get_key(std::map<int, int>* _map, int value);

	void populate_with_zeroes(double* _array, int dim_x, int dim_y);
		
	void populate_with_zeroes(uint* _array, int dim_x, int dim_y);

	void split(std::string basestring, std::string separator, vector<std::string>& substrings);

	pair<float, float> get_random_direction();

	//Return whether the given vector <vec> contains the integer <item>
	bool is_in(std::vector<int>* vec, int item);

	void print_map(std::map<int, int>* map);

	void print_vector(std::vector<int>* vec);

	void print_pairs(std::vector<pair<int, int>>* vec);

	void print(std::string);

	vector<size_t> FindAll(std::string basestring, std::string target);

	bool is_in(std::string basestring, std::string target);

	std::string replace_occurrences(std::string basestring, std::string toReplace, std::string replaceWith);

	double fisqrt(float n);

	void increment_key(std::map<std::string, int>* map, std::string key);

	float get_value(std::map<std::string, float>* map, std::string key);

	int get_value(std::map<std::string, int>* map, std::string key);

	int get_value(std::map<int, int>* map, int key);

	int get_value(std::map<int, double>* map, int key);

	float cubed(float val);

	int get_value(std::map<uint32_t, uint32_t>* map, uint32_t key);

	float get_rand_float(float min, float max);

	uint get_rand_uint(int min, int max);

	void remove(vector<int>* vec, int item);

	float get_sigmoid(float x, float x_min, float x_max, float x_stretch);

	template <typename T>
	T pop(vector<T>* vec, int idx);

	template <typename T, typename U>
	pair<T, U> pop(map<T, U>* map, int idx);

	float get_dist(pair<float, float> p1, pair<float, float> p2);

	std::string add_padding(std::string basestring, int version);

	std::string join_as_string(vector<int> numbers, std::string separator);

	std::string join_as_string(vector<float> numbers, std::string separator);

	std::string join_as_string(vector<pair<int, int>> numbers, std::string separator);

	std::string join(vector<std::string>* strings, std::string separator);

	std::string join(vector<std::string> strings, std::string separator);

	void remove_largest_vector(vector<vector<int>>* vectors, int& max_size);

	bool ends_with(std::string full_string, std::string ending);

	bool have_overlap(vector<int>* larger_vector, vector<int>* smaller_vector);

	// Push back the items in vec2 to the vector <result>
	void append_vector(vector<int>& result, vector<int>* vec2);

	// Push back the items in vec2 to the vector <result>
	void append_vector(vector<int>& result, vector<int> vec2);

	// Push back the items in vec2 to the vector <result>
	void append_vector(vector<pair<int, int>>& result, vector<pair<int, int>>* vec2);

	// Push back the items in vec2 to the vector <result>
	void append_vector(vector<pair<int, int>>& result, vector<pair<int, int>> vec2);

	// Push back the items in vec2 to the vector <result>
	void append_vector(vector<string>& result, vector<string>* vec2);

	// Get free RAM memory
	vector<float> get_free_memory();

	// Get stdev
	double get_stdev(vector<double>* distribution, double mean = -999999);

	// Get mean
	double get_mean(vector<double>* distribution);

	// Get maximum
	double get_max(vector<double>* distribution);

	// Get minimum
	template <typename T>
	double get_min(vector<T>* distribution);

	// Get exponential function value
	float exponential_function(float x, float a, float b, float c);

	// Solve for x in quadratic function f(x), given coefficients (a and b), constant c, and value y = f(x)
	float get_lowest_solution_for_quadratic(float y, float a, float b, float c);

	class LinearProbabilityModel {
	public:
		LinearProbabilityModel();
		LinearProbabilityModel(float _q1, float _q2, float _min, float _max);
		float linear_sample();
		float q1 = 0;
		float q2 = 0;
		float min = 0;
		float max = 0;
		float a = 0;
		float b = 0;
	};

	class ProbModelPiece {
	public:
		ProbModelPiece();
		ProbModelPiece(float _xmin, float _xmax, float _ymin, float _ymax);
		float intersect(float cdf_y);
		void rescale(float factor);
		float xmin = 0;
		float xmax = 0;
		float ymin = 0;
		float ymax = 0;
		float ysize = 0;
		float xsize = 0;
	};

	class PieceWiseLinearProbModel {
	public:
		PieceWiseLinearProbModel();
		PieceWiseLinearProbModel(float _xmax);
		virtual float pdf(float x);
		virtual float sample();
		void build();
		float xmax = 0;
		float resolution = 1000.0f;
		ProbModelPiece* cdf_pieces = 0;
	};
};
