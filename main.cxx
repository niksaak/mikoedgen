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
 * TODO: as an extention of the previous todo, Hunk::marshall() method
 *       should be reimplemented as an external function, which would
 *       be able to discard "@" instruction when hunks are sequential,
 *       to generate
 *           @0,2+ABC-BA
 *       instead of
 *           @0,2+ABC
 *           @3,5-BA
 *
 * TODO: documentation (!). I heard it takes a month for an ordinary human to
 *       fully understand the paper, so it will be useful to have.
 *
 * TODO: command line flags. For reading input from stdin and such.
 *
 * TODO: it would be cool and more convenient to represent a and b positions
 *       as a 2-dimensional points instead of separate values.
 *
 * TODO: incorporate optimizations described in part 4 of the paper.
 */

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

enum DiffOp {
	DIFF_INSERT,
	DIFF_DELETE,
	DIFF_NOP,
};

struct Snake {
	int astart, bstart;
	int amid, bmid;
	int aend, bend;

	bool is_valid() const {
		return	(astart <= amid && bstart <= bmid)
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

struct Hunk {
	int apos;
	int bpos;
	DiffOp op;
	int length;
	std::string data;

	// TODO: reimplement as external function processing the hunk in a
	//       context of other hunks.
	std::string marshall() const {
		std::string s;
		s += "@" + std::to_string(apos) + "," + std::to_string(bpos) + ":";

		if (op == DIFF_INSERT) {
			s += "+";
		} else if (op == DIFF_DELETE) {
			s += "-";
		} else {
			throw std::logic_error("Bad DiffOp");
		}

		s += std::to_string(length);
		s += ":";
		s += data;

		return std::move(s);
	}
};

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
std::vector<Snake> compute_snakes(
		const std::vector<std::unordered_map<int,int>> vs,
		const int alen, const int blen
) {
	std::vector<Snake> snakes;
	int apos = alen;
	int bpos = blen;
	for (int d = vs.size() - 1; apos > 0 || bpos > 0; d--) {
		auto v = vs[d];

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
	return std::move(snakes);
}

static
std::vector<Hunk> process_snakes(std::vector<Snake> snakes) {
	std::vector<Hunk> hunks;

	// TODO: this will look better as a for loop, I think.
	auto snake = snakes.cbegin();
	while (snake->diff_op() == DIFF_NOP || snake->bstart == -1) {
		snake++;
	}
	Hunk hunk = {
		snake->astart,
		snake->bstart,
		snake->diff_op(),
		1,
	};
	snake++;
	do {
		if (snake->diff_op() == DIFF_NOP || snake->bstart == -1) {
			snake++;
			continue;
		}

		if (hunk.op == DIFF_NOP || snake->diff_op() != hunk.op) {
			hunks.push_back(hunk);
			hunk.length = 0;
			hunk.op = snake->diff_op();
		}
		hunk.apos = snake->astart;
		hunk.bpos = snake->bstart;
		hunk.length++;

		snake++;
	} while(snake != snakes.cend());
	hunks.push_back(hunk);

	std::reverse(hunks.begin(), hunks.end());
	return std::move(hunks);
}

static
std::vector<Hunk> compute_hunks(std::string a, std::string b) {
	const int alen = a.size();
	const int blen = b.size();

	std::vector<std::unordered_map<int,int>> vs = compute_trace(a, b);

	std::vector<Snake> snakes = compute_snakes(vs, alen, blen);

	std::vector<Hunk> hunks = process_snakes(snakes);

	// а потім маленьке звірятко загортає все це у фольгу
	for (auto hunk = hunks.begin(); hunk != hunks.end(); hunk++) {
		if (hunk->op == DIFF_INSERT) {
			hunk->data = b.substr(hunk->bpos, hunk->length);
		} else {
			hunk->data = a.substr(hunk->apos, hunk->length);
		}
	}

	return std::move(hunks);
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		std::cout << "Usage:\n";
		std::cout << "  " << argv[0] << " <file1> <file2>";
		return -1;
	}

	std::ifstream file1;
	file1.exceptions(std::ifstream::failbit);
	file1.open(argv[1], std::ifstream::in | std::ifstream::binary);
	std::ifstream file2;
	file2.exceptions(std::ifstream::failbit);
	file2.open(argv[2], std::ifstream::in | std::ifstream::binary);

	std::string a(
			(std::istreambuf_iterator<char>(file1)),
			(std::istreambuf_iterator<char>())
	);
	std::string b(
			(std::istreambuf_iterator<char>(file2)),
			(std::istreambuf_iterator<char>())
	);

	std::cout << "-" << a;
	std::cout << "+" << b;

	auto hunks = compute_hunks(a, b);
	std::cout << "\nHunks:\n";
	for (auto hunk = hunks.cbegin(); hunk != hunks.cend(); ++hunk) {
		std::cout << hunk->marshall() << "\n";
	}
}

// vim: tabstop=8 softtabstop=8 shiftwidth=8 noexpandtab
