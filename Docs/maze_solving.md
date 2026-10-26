# Task 8 - Maze solving


Diagram: `docs/maze_example.svg` (annotated example maze).

## 1. DFS vs the wall-preference rule

| | Left/right-hand rule | DFS with a visited set |
|---|---|---|
| Memory and sensing | None; reacts to what it sees | A small map (about 1-3 KB for 100 nodes) and a way to know where it is |
| Works when.. | The goal is connected to the outer wall (a "simply connected" maze, no loops) | Any finite connected maze, including loops and islands |
| Failure case | Can circle forever around a loop or island | Costs time to physically backtrack; needs position tracking |
| Result | A path that includes dead ends and detours; no map | A complete map, so the best route can be computed afterwards |

**Choice: DFS.** The maze is unknown, so we cannot assume it has no loops. The wall rule is simpler, but it can fail completely on a maze with a loop, while DFS always terminates and leaves a map for the replay run.

Assumptions: junctions are 90 degrees; the goal is marked distinctly; wheel encoders measure edge lengths (the edges are not all the same length).

## 2. Mapping the maze to a graph

- **Nodes:** the start, every junction, every dead end and the goal. (A pure 90-degree corner can be merged into the edge; in the example, C is a corner kept as a node so the turn is explicit.)
- **Edges:** the line segments between nodes, each with a **length measured by the encoders**.
- **Localization:** the robot tracks its heading (it turns in 90-degree steps) and adds encoder distance along the heading to get an (x, y) estimate. At each junction it compares this with the stored nodes. A stored node within a few centimetres means "I have been here before" (a loop), and the estimate is snapped to that node's stored position to cancel drift.
- **A loop edge** is an edge that leads to an already-visited node other than the one we came from (a "back edge").

Data (about 17 bytes per node (two 16-bit coordinates, a flag, four edges of 3 bytes), so well within the ESP32's 520 KB): `Node { x, y, visited, edge[4] }` with `Edge { to, length }`, and an explicit stack of node ids instead of recursion.

## 3. Worked example (see the diagram)

Nodes S, A, B, C, D, E, G; edges S-A 20, A-B 30, A-D 150 (a long winding corridor), B-E 10, B-C 30, C-D 30, D-G 20 (cm). Exits are tried in alphabetical order for this example.

| Step | Move | Distance (cm) | Total (cm) | What happens |
|---|---|---|---|---|
| 1 | S to A | 20 | 20 | New node A |
| 2 | A to B | 30 | 50 | New node B |
| 3 | B to E | 10 | 60 | New node E: dead end |
| 4 | E to B | 10 | 70 | Backtrack |
| 5 | B to C | 30 | 100 | New node C |
| 6 | C to D | 30 | 130 | New node D |
| 7 | D toward A | 150 | 280 | Arrives at A, which is already visited: **loop edge D-A recorded** |
| 8 | A back to D | 150 | 430 | U-turn, return to D |
| 9 | D to G | 20 | 450 | Goal found; keep exploring anyway |
| 10-14 | G, D, C, B, A, S | 20+30+30+30+20 | 580 | Backtrack to the start; all exits explored, so the map is complete |

Exploration costs 580 cm, and the loop probe alone is 300 of it. The run **does not stop at the goal**: it ends when the stack is empty with every exit of every node explored, so the map is complete and the robot is back at the start.

## 4. Exploratory traversal vs optimal-route replay

**Exploration (unknown maze):** the robot must discover branches, remember what it has visited, detect loops, and backtrack. It needs localization and a stack.

**Replay (known map):** the map is complete, so the best route is computed once, before moving, and the robot just follows it. Replay is simpler because there is nothing left to discover: no branch detection, no visited checks, no backtracking and no localization. The robot line-follows (the same PID) and at each junction executes the next entry of a precomputed turn list.

**Which algorithm for the route:** with encoders the edges have different lengths, so the shortest route by **distance** is found with **Dijkstra** (BFS is the special case where every edge counts as 1). In the example:

- BFS (fewest junctions) gives S, A, D, G: 3 edges but 20 + 150 + 20 = **190 cm**.
- Dijkstra gives S, A, B, C, D, G: 5 edges but 20 + 30 + 30 + 30 + 20 = **130 cm**, which is 60 cm (32%) shorter.

The replay turn list for the green route is: at A turn left, at B right, at C right, at D left, then stop at G. The replay run drives 130 cm instead of 580 cm.

## 5. Pseudocode

**Exploration (iterative DFS):**

```
create node S at (0, 0); mark visited; stack = [S]; current = S
loop:
    u = current
    if u has an unexplored exit e (fixed order, e.g. left, straight, right):
        mark e explored at u
        drive along e (line-follow); d = encoder distance
        update (x, y) from heading and d
        v = stored node within tolerance of (x, y), if any
        if v exists:                          # already known: a loop
            add_edge(u, v, d); mark v's matching exit explored
            U-turn; drive d back to u         # stay at u
        else:
            v = new node at (x, y); add_edge(u, v, d)
            read v's exits from the sensors; mark the exit we arrived by as explored
            if v is the goal: mark v as goal
            push v; current = v               # go deeper
    else:                                     # all exits of u explored
        pop u
        if stack is empty: stop               # back at the start, map complete
        current = top of stack
        drive back along the edge from u to current
```

**Route (Dijkstra, run once on the stored map):**

```
dist[all] = infinity; dist[S] = 0; prev[all] = none; unvisited = all nodes
repeat:
    u = unvisited node with smallest dist
    if u is G: break
    remove u from unvisited
    for each edge (u, v, length):
        if dist[u] + length < dist[v]: dist[v] = dist[u] + length; prev[v] = u
route = follow prev[] from G back to S, reversed
turns = for each node on the route, compare incoming heading with outgoing heading -> L / S / R
```

**Replay:**

```
for each node on the route:
    line-follow to the next junction
    execute the next turn from the list (left / straight / right)
stop at G
```

## 6. Limitations

- Encoder drift and wheel slip make the position estimate imperfect; the tolerance and snap-to-node correction reduce this but can fail on a very large maze.
- The 90-degree assumption: other angles would need a gyro or differential-encoder heading.
- The 150 cm loop probe is expensive; a real implementation could stop probing an edge once the encoder position already matches a known node.
- Not tested on a physical maze: the example is hand-worked.
