# Task 8 - Maze solving

> DRAFT - rewrite this in your own words and check every number before submitting. You will be asked to defend each line.

Diagram: the annotated maze drawing for the example in section 3 (hand-drawn, in `docs/`).

## 1. DFS vs the wall-preference rule

| | Left/right-hand rule | DFS with a visited set |
|---|---|---|
| Memory and sensing | None; reacts to what it sees | A small map (about 2 KB for 100 nodes) and a position estimate |
| Works when... | The goal is connected to the outer wall (a "simply connected" maze, no loops) | Any finite connected maze, including loops and islands |
| Failure case | Can circle forever around a loop or island, and never knows it is finished | Costs time to physically backtrack; needs position tracking |
| Result | A path that includes dead ends and detours; no map | A complete map, so the best route can be computed afterwards |

**Choice: DFS with a left-first exit order.** The maze is unknown, so we cannot assume it has no loops. At every junction the robot tries the unexplored exits in the order left, straight, right (the left-hand rule's order); what DFS adds is memory, which lets it recognise a node it has seen before and know when the whole maze is explored.

Assumptions: junction angles do not have to be exactly 90 degrees, because branches are only classified as left, straight or right from the sensor pattern; the goal is marked distinctly (a large black area); both wheels have encoders, and the wheel diameter and track width are measured on the built robot.

## 2. Mapping the maze to a graph

- **Nodes:** the start, every junction (where the line splits), every dead end and the goal. A bend with no branch is **not** a node: it is part of an edge, and the PID follows it.
- **Edges:** the line segments between nodes, each with a length measured by the encoders.
- **Position:** from the two encoders, distance travelled is the average of the wheels and the heading change is `(right distance - left distance) / track width`. This works on curves and on corners that are not exactly 90 degrees. Drift accumulates, so on arriving at a junction the robot compares its (x, y) estimate with the stored nodes: within a few centimetres of one means "known node", and the estimate is snapped to that node's stored position.
- **Loop edge:** an edge that leads to an already-visited node other than the one we came from.
- **Data:** `Node { x, y, visited, exits[4] }`, with the exits kept in order (left, straight, right) from the direction the robot first entered, each with `{ to, length, explored }`. That is about 20 bytes per node, plus an explicit stack of node ids instead of recursion.

**Detecting junctions, dead ends and the goal** (planned, not yet in the code), using the five sensors S1-S5:

- **Line:** one to three adjacent sensors on, near the centre: the PID follows it. This covers curves and bends.
- **Branch on a side:** S1 (or S5) turns on while the line is still under the centre: latch "branch on the left" (or right).
- **Straight exit:** after a branch or a full-width bar is seen, drive a fixed encoder distance so the wheel axle is over the junction, then re-read: if the centre sensors still see a line, a straight exit exists.
- **Dead end versus sharp corner:** the line vanishes. If the last error was near zero (the line ended straight ahead), it is a dead end and the robot U-turns. If the last error was at the edge (above about 1.5), it is a sharp corner and the robot pivots toward the side where the line was last seen.
- **Goal:** all five sensors on for a sustained distance, longer than a junction bar.

**Turning is closed-loop.** There are no fixed-angle turns. To take a chosen exit the robot pivots toward it until the centre sensors see the line again (centroid near 0), then the normal PID resumes; a timeout stops it if the line is never found. The PID alone is not enough for a sharp junction turn, because its error is limited to 2, which caps the steering difference at about 80 PWM counts; it handles bends, and the pivot handles sharp turns.

## 3. Worked example

Nodes: S (start), A, B, D, E (dead end), G (goal). Edges (cm): S-A 20, A-B 30, B-E 10, B-D 60 (includes a bend), A-D 150 (a long winding corridor, which forms a loop), D-G 20.

Rule: at every junction, try the unexplored exits in the order **left, straight, right**, measured from the direction the robot first entered that junction. The letters are only labels for us; the robot numbers nodes in the order it discovers them.

| Step | Move | Turn at the start of the move | Distance (cm) | Total (cm) | What happens |
|---|---|---|---|---|---|
| 1 | S to A | follow the line | 20 | 20 | New node A. Exits: left (to B), right (corridor to D) |
| 2 | A to B | left (first in left-first order) | 30 | 50 | New node B. Exits: straight (E), right (to D) |
| 3 | B to E | straight | 10 | 60 | New node E: the line ends ahead, so it is a dead end; U-turn |
| 4 | E to B | straight | 10 | 70 | Back at B; next unexplored exit leads to D |
| 5 | B to D | left | 60 | 130 | New node D (the bend is handled by the PID). Exits: left (G), straight (corridor to A) |
| 6 | D to G | left | 20 | 150 | Goal marker found; dead end; U-turn; goal recorded, but exploration continues |
| 7 | G to D | straight | 20 | 170 | Back at D; the corridor to A is the next unexplored exit |
| 8 | D to A | left | 150 | 320 | Arrives at A, a node already visited (position matches): **loop edge D-A recorded** |
| 9 | A to S | left | 20 | 340 | Every exit of A, B and D is explored, so the robot continues from A instead of driving back to D; it reaches S and exploration is complete |

Exploration costs 340 cm, of which the loop probe is 150. (If the robot U-turned after step 8 and walked back through D, B and A, it would cost 580 cm.) The run does not stop at G: it ends back at S with every exit of every node explored.

For contrast, the plain left-hand rule uses the same exit order but has no memory. After step 8 it would take the exit to S, turn around, return to A and go round the whole maze again forever, never knowing it had finished. The visited-node memory is what lets the robot recognise A and stop.

## 4. Exploratory traversal vs optimal-route replay

**Exploration (unknown maze):** the robot must discover branches, build the map, remember what it has visited, recognise loops, and backtrack. It needs position tracking and a stack.

**Replay (known map):** the best route is computed once, before moving. Replay is simpler because there is nothing left to discover: the robot does not build a map, check visited nodes, backtrack or track its position. It still recognises each junction (the same sensor patterns), but it just takes the exit its route says. For each node on the route, the exit is stored as "the k-th exit counting from the left of the direction of arrival", worked out from the node's stored exit order, so it stays correct even if the route enters a node by a different edge than exploration did.

**Which algorithm for the route:** with encoders the edges have different lengths, so the shortest route by **distance** is found with **Dijkstra** (BFS is the special case where every edge counts as 1). In the example:

- BFS (fewest junctions) gives S, A, D, G: 3 edges but 20 + 150 + 20 = **190 cm**.
- Dijkstra gives S, A, B, D, G: 4 edges but 20 + 30 + 60 + 20 = **130 cm**, which is 60 cm (32%) shorter.

The replay turns are: left at A, right at B, left at D (the 1st, 2nd and 1st exit counting from the left), then stop at G. The replay run drives 130 cm instead of the 340 cm of exploration.

## 5. Pseudocode

**Exploration (iterative DFS, left-first):**

```
create node S at (0, 0); mark visited; stack = [S]; current = S
loop:
    u = current
    e = next unexplored exit of u in order (left, straight, right) from u's entry direction
    if e exists:
        mark e explored; pivot toward e until the line is found, then PID
        line-follow to the next junction / dead end / goal; d = encoder distance
        update (x, y, heading) from the encoders
        v = stored node within tolerance of (x, y), if any
        if v exists:                          # already known: a loop
            add_edge(u, v, d); mark v's matching exit explored
            if v is on the stack and every node above v has no unexplored exits:
                pop the stack down to v; current = v      # continue from here
            else:
                U-turn; drive d back to u                 # stay at u
        else:
            v = new node at (x, y); add_edge(u, v, d)
            detect v's exits; order them (left, straight, right) from the entry edge
            mark the exit we arrived by as explored
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
for each node on the route: k = position of the outgoing exit, counting from the left of the arrival direction
```

**Replay:**

```
for each node on the route (except G):
    line-follow to the next junction; detect the exits
    take the k-th exit from the left, using the stored k
stop at G
```

## 6. Limitations

- Encoder drift and wheel slip make the position estimate imperfect; the tolerance and snap-to-node correction reduce this, but may fail on a very large maze.
- Classifying junctions with only five sensors is approximate; two extra outer sensors would make it more reliable.
- **Not implemented or tested:** the junction, dead-end and goal detection above. A U-turn at the recovery speed takes roughly 0.6 s (assuming an 8 cm track width), about the same as the sketch's 600 ms stop timeout, so a dead-end U-turn needs its own state or a longer timeout. The sketch also stores only the sign of the last error, but the dead-end test needs its size.
- The example is worked by hand, not run on a physical maze.
