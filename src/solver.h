/*
=================================================================================================
FILE: src/solver.h

DESCRIPTION:
This file contains the logic engine for the Thrill Digger Calculator.
It implements the probability calculation algorithm that determines how likely any given
cell is to contain a "Bad" item (Bomb or Rupoor).

IMPORTANCE:
This is the core "brain" of the software. Without this file, the UI would have nothing to display.
It transforms the state of the board (what the user has seen) into actionable probabilities.

INTERACTION:
- Included by `src/main.cpp`.
- The `ThrillDiggerSolver` class is instantiated as a global object in main.cpp.
- The `solve()` method is called every time the user updates a cell.

ALGORITHM OVERVIEW:
This is a constraint satisfaction problem solver.
1. It identifies "components": groups of unknown cells connected by shared clues.
2. For each component, it enumerates all valid configurations of bad items that satisfy the clues.
3. It counts how many times a cell is "bad" across all valid configurations.
4. It combines results from independent components using polynomial convolution to account for the
   global count of remaining bad items.
5. Finally, it computes the % chance for each cell.
=================================================================================================
*/

#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <cmath>

// =================================================================================================
// CONFIGURATION
// =================================================================================================

// Grid dimensions for Expert mode of Thrill Digger
constexpr int ROWS = 5;
constexpr int COLS = 8;
constexpr int TOTAL_CELLS = ROWS * COLS; // Total 40 cells
constexpr int TOTAL_BAD = 16;            // Expert mode has 8 bombs + 8 rupoors = 16 bad items

// =================================================================================================
// DATA STRUCTURES
// =================================================================================================

// Enum representing the content of a cell.
// uint8_t is used to save memory.
enum class CellContent : uint8_t {
    Undug = 0, // Unknown/Hidden
    Green,     // Revealed: Green Rupee (Indicates 0 bad neighbors)
    Blue,      // Revealed: Blue Rupee  (Indicates 1-2 bad neighbors)
    Red,       // Revealed: Red Rupee   (Indicates 3-4 bad neighbors)
    Silver,    // Revealed: Silver Rupee(Indicates 5-6 bad neighbors)
    Gold,      // Revealed: Gold Rupee  (Indicates 7-8 bad neighbors)
    Rupoor,    // Revealed: Rupoor (Is a bad item)
    Bomb       // Revealed: Bomb (Is a bad item)
};

/*
 * badNeighborRange
 * ----------------
 * Returns the rule for a specific rupee type.
 * e.g., Blue Rupee implies there are between 1 and 2 bad items surrounding it.
 * pair.first = min, pair.second = max.
 */
inline std::pair<int,int> badNeighborRange(CellContent c) {
    switch (c) {
        case CellContent::Green:  return {0, 0};
        case CellContent::Blue:   return {1, 2};
        case CellContent::Red:    return {3, 4};
        case CellContent::Silver: return {5, 6};
        case CellContent::Gold:   return {7, 8};
        default: return {-1, -1}; // Not a clue giver
    }
}

// Checks if the content is a Rupee (Green through Gold)
inline bool isRevealedGood(CellContent c) {
    return c >= CellContent::Green && c <= CellContent::Gold;
}

// Checks if the content is a Bad item (Bomb or Rupoor)
inline bool isRevealedBad(CellContent c) {
    return c == CellContent::Rupoor || c == CellContent::Bomb;
}

// Checks if the user has marked this cell as anything other than Undug
inline bool isRevealed(CellContent c) {
    return c != CellContent::Undug;
}

/*
 * UnionFind
 * ---------
 * A standard Disjoint-Set Data Structure.
 * Used here to group cells into connected "components" that influence each other.
 * If Cell A is near Clue 1, and Cell B is near Clue 1, A and B are connected.
 */
struct UnionFind {
    std::vector<int> parent;
    UnionFind(int n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    
    // Find representative of the set
    int find(int x) { return parent[x] == x ? x : parent[x] = find(parent[x]); }
    
    // Merge two sets
    void unite(int a, int b) { a = find(a); b = find(b); if (a != b) parent[a] = b; }
};

/*
 * Constraint Structures
 * ---------------------
 * These structures hold the rules we need to satisfy.
 */

// A constraint derived from a revealed rupee on the board.
struct Constraint {
    std::vector<int> frontierLocalIdx; // Indices of the unknown neighbors involved
    int minBad, maxBad;                // The rule (e.g., must have 1 to 2 bad items)
};

// Same as Constraint, but remapped to be specific to a connected component.
struct LocalConstraint {
    std::vector<int> localIdx;
    int minBad, maxBad;
};

// The result of analyzing one connected component.
struct ComponentResult {
    int size;                                      // Number of unknown cells in this component
    std::vector<double> counts;                    // counts[k] = number of ways to place exactly k bad items in this component
    std::vector<std::vector<double>> badCounts;    // badCounts[i][k] = how many times cell i is bad when total bad is k
    std::vector<int> globalIndices;                // Maps local index back to the global board index
};

// Helper: Calculate combinations "n choose k"
static double binomial(int n, int k) {
    if (k < 0 || k > n) return 0.0;
    if (k == 0 || k == n) return 1.0;
    double result = 1.0;
    if (k > n - k) k = n - k;
    for (int i = 0; i < k; i++) {
        result *= (n - i);
        result /= (i + 1);
    }
    return result;
}

// =================================================================================================
// MAIN SOLVER CLASS
// =================================================================================================
class ThrillDiggerSolver {
public:
    // The current state of the board (what the user sees)
    std::array<CellContent, TOTAL_CELLS> grid;
    
    // The calculated output: probability (0.0 to 1.0) of each cell being Bad
    std::array<double, TOTAL_CELLS> badProb;

    ThrillDiggerSolver() { reset(); }

    /*
     * reset
     * -----
     * Clears the board and sets probabilities to the baseline average.
     */
    void reset() {
        grid.fill(CellContent::Undug);
        double prior = static_cast<double>(TOTAL_BAD) / TOTAL_CELLS; // e.g., 16/40 = 0.4
        badProb.fill(prior);
    }

    // Update a single cell's content
    void setCell(int row, int col, CellContent content) {
        grid[row * COLS + col] = content;
    }

    /*
     * getNeighbors
     * ------------
     * Returns a list of cell indices (0..39) surrounding a specific cell.
     * Handles boundary checks (corners, edges).
     */
    static std::vector<int> getNeighbors(int idx) {
        int r = idx / COLS, c = idx % COLS;
        std::vector<int> nbrs;
        nbrs.reserve(8);
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue; // Skip self
                int nr = r + dr, nc = c + dc;
                if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS)
                    nbrs.push_back(nr * COLS + nc);
            }
        return nbrs;
    }

    /*
     * enumerateComponent
     * ------------------
     * A recursive backtracking function.
     * It tries every possible combination of Bad/Safe for the cells in a component
     * to see if they satisfy the local clues.
     * 
     * If a valid configuration is found, it records stats in `counts` and `badCnts`.
     */
    static void enumerateComponent(
        int pos, int numBad, int compSize, int remainingBad,
        const std::vector<int>& order,
        const std::vector<int>& orderPos,
        std::vector<int>& assignment,
        const std::vector<std::vector<int>>& cellConstraints,
        const std::vector<LocalConstraint>& localConstraints,
        std::vector<double>& counts,
        std::vector<std::vector<double>>& badCnts)
    {
        // Optimization: Stop if we've already used more bad items than exist globally
        if (numBad > remainingBad) return;

        // Base Case: All cells in component assigned
        if (pos == compSize) {
            counts[numBad] += 1.0; // Valid configuration found with `numBad` items
            for (int i = 0; i < compSize; i++) {
                if (assignment[i]) {
                    badCnts[i][numBad] += 1.0; // Record that cell i was bad in this config
                }
            }
            return;
        }

        int cell = order[pos]; // Pick next cell to assign based on optimization order

        // Try assigning 0 (Safe) and 1 (Bad)
        for (int val = 0; val <= 1; val++) {
            assignment[cell] = val;
            int newBad = numBad + val;

            // Check if this assignment violates any constraints immediately
            bool valid = true;
            for (int ci : cellConstraints[cell]) {
                const auto& lc = localConstraints[ci];
                int badCount = 0, unassignedCount = 0;
                
                // Count bad items among neighbors processed so far
                for (int li : lc.localIdx) {
                    if (orderPos[li] <= pos) {
                        badCount += assignment[li];
                    } else {
                        unassignedCount++;
                    }
                }
                
                // Pruning:
                // 1. Too many bad items? (Already exceeded max)
                if (badCount > lc.maxBad) { valid = false; break; }
                
                // 2. Too few bad items? (Even if all remaining neighbors are bad, can't reach min)
                if (unassignedCount == 0) {
                    if (badCount < lc.minBad) { valid = false; break; }
                } else {
                    if (badCount + unassignedCount < lc.minBad) { valid = false; break; }
                }
            }

            if (valid) {
                // Recurse
                enumerateComponent(pos + 1, newBad, compSize, remainingBad,
                    order, orderPos, assignment, cellConstraints,
                    localConstraints, counts, badCnts);
            }
        }
        assignment[cell] = 0; // Backtrack cleanup
    }

    /*
     * convolve
     * --------
     * Polynomial multiplication.
     * Used to combine probability distributions.
     * If Component A has ways {1, 2} to have {0, 1} bombs,
     * and Component B has ways {3, 1} to have {0, 1} bombs,
     * convolve gives the ways for the combined set.
     */
    static std::vector<double> convolve(const std::vector<double>& a, const std::vector<double>& b) {
        if (a.empty() || b.empty()) return {};
        std::vector<double> result(a.size() + b.size() - 1, 0.0);
        for (size_t i = 0; i < a.size(); i++)
            for (size_t j = 0; j < b.size(); j++)
                result[i + j] += a[i] * b[j];
        return result;
    }

    /*
     * solve
     * -----
     * The main entry point for calculation.
     */
    void solve() {
        // Step 1: Classification
        // Identify which cells are definitely bad, which are clues, and which are unknown.
        std::vector<int> unknownCells;
        std::vector<int> constraintCells; // Cells that provide clues
        int knownBad = 0;

        for (int i = 0; i < TOTAL_CELLS; i++) {
            if (isRevealedBad(grid[i])) {
                badProb[i] = 1.0; // Known bad = 100%
                knownBad++;
            } else if (isRevealedGood(grid[i])) {
                badProb[i] = 0.0; // Known safe (clue) = 0%
                constraintCells.push_back(i);
            } else {
                unknownCells.push_back(i);
            }
        }

        int remainingBad = TOTAL_BAD - knownBad;

        // Trivial cases
        if (unknownCells.empty()) return; // Nothing to solve
        if (remainingBad <= 0) {
            // Found all bad items! Everything else is safe.
            for (int idx : unknownCells) badProb[idx] = 0.0;
            return;
        }
        if (constraintCells.empty()) {
            // No clues? Just use uniform probability.
            double p = static_cast<double>(remainingBad) / unknownCells.size();
            for (int idx : unknownCells) badProb[idx] = p;
            return;
        }

        // Step 2: Separation
        // "Frontier" cells = unknown cells touching a clue.
        // "Interior" cells = unknown cells NOT touching any clue.
        std::unordered_set<int> unknownSet(unknownCells.begin(), unknownCells.end());
        std::unordered_set<int> frontierSet;

        for (int ci : constraintCells) {
            for (int n : getNeighbors(ci)) {
                if (unknownSet.count(n)) frontierSet.insert(n);
            }
        }

        std::vector<int> frontier(frontierSet.begin(), frontierSet.end());
        std::sort(frontier.begin(), frontier.end());
        std::vector<int> interior;
        for (int idx : unknownCells) {
            if (!frontierSet.count(idx)) interior.push_back(idx);
        }

        int numFrontier = (int)frontier.size();
        int numInterior = (int)interior.size();

        // Step 3: Build Constraints
        // Convert the board state into mathematical rules (minBad, maxBad for lists of cells).
        std::vector<Constraint> constraints;
        for (int ci : constraintCells) {
            auto range = badNeighborRange(grid[ci]);
            int minB = range.first, maxB = range.second;
            auto nbrs = getNeighbors(ci);
            int knownBadN = 0;
            std::vector<int> fnbrs;
            
            for (int n : nbrs) {
                if (isRevealedBad(grid[n])) {
                    knownBadN++; // Count bad items already found
                } else if (frontierSet.count(n)) {
                    // Map global index to frontier index
                    auto it = std::lower_bound(frontier.begin(), frontier.end(), n);
                    fnbrs.push_back((int)(it - frontier.begin()));
                }
            }
            
            // Adjust requirements based on already found bad items
            int adjMin = std::max(0, minB - knownBadN);
            int adjMax = maxB - knownBadN;
            if (adjMax < 0) adjMax = 0;
            
            // Clamp to number of available neighbors
            adjMax = std::min(adjMax, (int)fnbrs.size());
            adjMin = std::min(adjMin, (int)fnbrs.size());

            if (!fnbrs.empty()) {
                Constraint con;
                con.frontierLocalIdx = fnbrs;
                con.minBad = adjMin;
                con.maxBad = adjMax;
                constraints.push_back(con);
            }
        }

        // Step 4: Partition into Components
        // Use Union-Find to group variables that interact with each other.
        UnionFind uf(numFrontier);
        for (const auto& con : constraints) {
            for (int i = 1; i < (int)con.frontierLocalIdx.size(); i++) {
                uf.unite(con.frontierLocalIdx[0], con.frontierLocalIdx[i]);
            }
        }

        std::unordered_map<int, std::vector<int>> components;
        for (int i = 0; i < numFrontier; i++) {
            components[uf.find(i)].push_back(i);
        }

        // Step 5: Solve Each Component Independently
        std::vector<ComponentResult> compResults;

        for (auto& kv : components) {
            int root = kv.first;
            auto& members = kv.second;
            int compSize = (int)members.size();

            // Create a local mapping (0..compSize) for the backtrack solver
            std::unordered_map<int, int> globalToLocal;
            for (int i = 0; i < compSize; i++) {
                globalToLocal[members[i]] = i;
            }

            // Filter constraints relevant to this component
            std::vector<LocalConstraint> localConstraints;
            for (const auto& con : constraints) {
                bool relevant = false;
                for (int fi : con.frontierLocalIdx) {
                    if (uf.find(fi) == root) { relevant = true; break; }
                }
                if (!relevant) continue;

                LocalConstraint lc;
                lc.minBad = con.minBad;
                lc.maxBad = con.maxBad;
                for (int fi : con.frontierLocalIdx) {
                    auto it = globalToLocal.find(fi);
                    if (it != globalToLocal.end()) {
                        lc.localIdx.push_back(it->second);
                    }
                }
                localConstraints.push_back(lc);
            }

            // Prepare for enumeration
            std::vector<double> counts(compSize + 1, 0.0);
            std::vector<std::vector<double>> badCnts(compSize, std::vector<double>(compSize + 1, 0.0));

            // Heuristic optimization: Sort cells by how constrained they are
            if (compSize <= 40) { 
                std::vector<int> assignment(compSize, 0);

                std::vector<std::vector<int>> cellConstraints(compSize);
                for (int ci = 0; ci < (int)localConstraints.size(); ci++) {
                    for (int li : localConstraints[ci].localIdx) {
                        cellConstraints[li].push_back(ci);
                    }
                }

                std::vector<int> order(compSize);
                std::iota(order.begin(), order.end(), 0);
                std::sort(order.begin(), order.end(), [&](int a, int b) {
                    return cellConstraints[a].size() > cellConstraints[b].size();
                });

                std::vector<int> orderPos(compSize);
                for (int i = 0; i < compSize; i++) orderPos[order[i]] = i;

                // RUN BACKTRACKING
                enumerateComponent(0, 0, compSize, remainingBad,
                    order, orderPos, assignment, cellConstraints,
                    localConstraints, counts, badCnts);
            } else {
                // Should not happen on standard board, but fallback just in case
                double p = static_cast<double>(remainingBad) / (int)unknownCells.size();
                for (int i = 0; i < compSize; i++) {
                    badProb[frontier[members[i]]] = p;
                }
                ComponentResult cr;
                cr.size = compSize;
                cr.globalIndices = members;
                compResults.push_back(cr);
                continue;
            }

            ComponentResult cr;
            cr.size = compSize;
            cr.counts = counts;
            cr.badCounts = badCnts;
            cr.globalIndices = members;
            compResults.push_back(cr);
        }

        // Step 6: Global Combination
        // We know how many ways each component can have X bad items.
        // We must combine these to match the TOTAL bad items remaining globally.
        int numComps = (int)compResults.size();

        // Calculate distribution for interior (free) cells using binomial coeffs
        std::vector<double> interiorPoly(numInterior + 1);
        for (int m = 0; m <= numInterior; m++) {
            interiorPoly[m] = binomial(numInterior, m);
        }

        // Convolve everything together to get total valid configurations
        std::vector<double> compProd = {1.0};
        for (int i = 0; i < numComps; i++) {
            if (compResults[i].counts.empty()) continue;
            compProd = convolve(compProd, compResults[i].counts);
        }

        std::vector<double> totalPoly = convolve(interiorPoly, compProd);

        // How many valid worlds exist with exactly `remainingBad` items?
        double totalWays = (remainingBad < (int)totalPoly.size()) ? totalPoly[remainingBad] : 0.0;

        if (totalWays <= 0.0) {
            // Contradiction detected (user made a mistake?). Fallback.
            double p = static_cast<double>(remainingBad) / (int)unknownCells.size();
            for (int idx : unknownCells) badProb[idx] = p;
            return;
        }

        // Step 7: Final Probability Calculation for Frontier Cells
        for (int ci = 0; ci < numComps; ci++) {
            auto& cr = compResults[ci];
            if (cr.counts.empty()) continue;

            // Calculate combinations for "everything EXCEPT this component"
            std::vector<double> withoutComp = {1.0};
            for (int j = 0; j < numComps; j++) {
                if (j == ci || compResults[j].counts.empty()) continue;
                withoutComp = convolve(withoutComp, compResults[j].counts);
            }
            std::vector<double> totalWithout = convolve(withoutComp, interiorPoly);

            for (int li = 0; li < cr.size; li++) {
                int frontierIdx = cr.globalIndices[li];
                int globalIdx = frontier[frontierIdx];

                double numerator = 0.0;
                // Sum configurations where this cell is bad
                for (int k = 0; k <= cr.size && k <= remainingBad; k++) {
                    int rest = remainingBad - k; // Items remaining for rest of board
                    if (rest >= 0 && rest < (int)totalWithout.size()) {
                        numerator += cr.badCounts[li][k] * totalWithout[rest];
                    }
                }
                badProb[globalIdx] = numerator / totalWays;
            }
        }

        // Step 8: Probability for Interior Cells
        if (numInterior > 0) {
            double interiorNumerator = 0.0;
            for (int m = 1; m <= numInterior && m <= remainingBad; m++) {
                int rest = remainingBad - m;
                if (rest >= 0 && rest < (int)compProd.size()) {
                    // interior cells pick m items, rest pick remainder
                    interiorNumerator += binomial(numInterior - 1, m - 1) * compProd[rest];
                }
            }
            double interiorProb = interiorNumerator / totalWays;
            for (int idx : interior) {
                badProb[idx] = interiorProb;
            }
        }

        // Clamp probabilities to be safe
        for (int i = 0; i < TOTAL_CELLS; i++) {
            if (badProb[i] < 0.0) badProb[i] = 0.0;
            if (badProb[i] > 1.0) badProb[i] = 1.0;
        }
    }
};
