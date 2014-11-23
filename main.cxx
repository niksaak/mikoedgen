/* MIKOEDGEN
 *
 * Mikola Samardak's Edit Script Generator, implemented as a dare,
 * using Myers's algorithm from
 * „An O(ND) Difference Algorithm and Its Variations“.
 *
 * Currently suits better for binary, mostly because output can
 * hardly be considered human-readable.
 * Output script consists of sequence of instructions and raw data
 * to be inserted or deleted.
 * Format:
 *   @a,b: - jump to offsets a, b for first and second string respectively.
 *   +c:A - insert array A of length c.
 *   -c:A - delete array A of length c.
 *
 * TODO: compact together sequences of hunks with the same operation.
 *
 * TODO: it would be cool and more convenient to represent a and b positions
 *       as a 2-dimensional points instead of separate values.
 *
 * TODO: incorporate optimizations described in part 4 of the paper.
 */

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>
#include <vector>

enum DiffOp {
	DIFF_INSERT,
	DIFF_DELETE,
	DIFF_NOP,
};

struct Hunk {
	int apos;
	int bpos;
	DiffOp op;
	int length;
};

struct Snake {
	int astart, bstart;
	int amid, bmid;
	int aend, bend;

	bool is_valid() const {
		return     (astart <= amid && bstart <= bmid)
			&& ((aend - amid) == (bend - bmid));
	}

	bool has_diagonal() const {
		return (amid == aend) && (bend == bmid);
	}

	DiffOp diff_op() const {
		if (astart < amid) {
			return DIFF_DELETE;
		} else if (bstart < bmid) {
			return DIFF_INSERT;
		} else {
			return DIFF_NOP;
		}
	}
};

static
std::ostream& operator << (std::ostream& s, const Snake& snake) {
	s << "Snake{";
	s << std::to_string(snake.astart) + ",";
	s << std::to_string(snake.bstart) + ",";
	s << std::to_string(snake.amid) + ",";
	s << std::to_string(snake.bmid) + ",";
	s << std::to_string(snake.aend) + ",";
	s << std::to_string(snake.bend);
	s << "}";
	return s;
}

static
std::vector<Hunk> process_snakes(std::vector<Snake> snakes) {
	std::vector<Hunk> hunks;
	Hunk hunk;
	for (auto snake = snakes.cbegin(); snake != snakes.cend(); snake--) {
		if (snake->bstart == -1) {
			continue;
		}

		DiffOp op = snake->diff_op();
		if (op == DIFF_NOP) {
			continue;
		}

		if (op == hunk.op && !snake->has_diagonal()) {
			hunk.apos = snake->astart;
			hunk.bpos = snake->bstart;
			hunk.length++;
		} else {
			hunks.push_back(hunk);
			hunk = Hunk{
				snake->astart,
				snake->bstart,
				op,
				1,
			};
		}
	}
	std::reverse(hunks.begin(), hunks.end());
	return std::move(hunks);
}

static
std::vector<std::unordered_map<int,int>> compute_trace(std::string a, std::string b) {
	const int alen = a.size();
	const int blen = b.size();

	std::vector<std::unordered_map<int,int>> vs;
	std::unordered_map<int,int> v;
	v[1] = 0;
	for (int d = 0; d <= alen + blen; d++) {
		bool solved = false;
		for (int k = -d; k <= d; k += 2) {
			int apos;
			if (k == -d || (k != d && v[k-1] < v[k+1])) {
				apos = v[k+1];
			} else {
				apos = v[k-1]+1;
			}
			int bpos = apos - k;
			while (apos < alen && bpos < blen && a[apos] == b[bpos]) {
				apos++;
				bpos++;
			}
			v[k] = apos;
			if (apos >= alen && bpos >= blen) {
				solved = true;
				break;
			}
		}
		vs.push_back(std::unordered_map<int,int>(v));

		if (solved) {
			return std::move(vs);
		}
	}

	throw std::logic_error("Unable to solve trace. Should actually never happen >vv<");
}

static
std::vector<Hunk> compute_hunks(std::string a, std::string b) {
	const int alen = a.size();
	const int blen = b.size();

	std::vector<std::unordered_map<int,int>> vs;

	// find shortest D-path
	std::unordered_map<int,int> v;
	v[1] = 0;
	for (int d = 0; d <= alen + blen; d++) {
		bool solved = false;
		std::cout << "d: " << d << "\n";
		for (int k = -d; k <= d; k += 2) {
			int apos;
			int bpos;
			if (k == -d || (k != d && v[k-1] < v[k+1])) {
				apos = v[k+1];
			} else {
				apos = v[k-1]+1;
			}
			bpos = apos - k;
			while (apos < alen && bpos < blen && a[apos] == b[bpos]) {
				apos++;
				bpos++;
			}
			v[k] = apos;
			std::cout << "v[" << k << "] = (" << apos << "," << bpos << ")\n";
			if (apos >= alen && bpos >= blen) {
				std::cout << "found solution of length " << d << "\n";
				solved = true;
				break;
			}
		}
		vs.push_back(std::unordered_map<int,int>(v));

		if (solved) {
			break;
		}
	}

	// calculate snakes starting at the last endpoint
	std::vector<Snake> snakes;
	int apos = alen;
	int bpos = blen;
	for (int d = vs.size() - 1; apos > 0 || bpos > 0; d--) {
		auto v = vs[d];

		std::cout << "d: " << d << "\n";
		std::cout << "pos: (" << apos << "," << bpos << ")\n";

		int k = apos - bpos;

		// end
		int aend = v[k];
		int bend = apos - k;

		bool down = (k == -d || (k != d && v[k-1] < v[k+1]));
		int kprev = (down) ? k+1 : k-1;

		// start
		int astart = v[kprev];
		int bstart = astart - kprev;

		// middle
		int amid = (down) ? astart : astart + 1;
		int bmid = amid - k;

		Snake snake{astart, bstart, amid, bmid, aend, bend};
		snakes.push_back(snake);

		apos = astart;
		bpos = bstart;
	}

	return process_snakes(snakes);
}

int main(void) {
	std::string a = "CABCABBA";
	std::string b = "CBABACA";
	auto snakes = compute_snakes(a, b);
	std::cout << "\nSnakes:\n";
	for (auto snake = snakes.begin(); snake != snakes.end(); ++snake) {
		std::cout << *snake << "\n";
	}
}

// vim: tabstop=8 softtabstop=8 shiftwidth=8 noexpandtab
