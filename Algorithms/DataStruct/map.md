# 图

[TOC]

## 基本概念

图是一种非线性数据结构，由顶点（vertex）和边（edge）组成。

图的类型：

- 无向图：边表示顶点之间的双向连接关系
- 有向图：边具有方向
- 有权图：边具有权重

图的属性：

- 邻接（adjacency）：表示两个顶点通过边连接
- 路径：从顶点 A 到顶点 B 经过的边都是 A 到 B 的路径。
- 度（degree）：顶点拥有的边数，有向图中入度是指向该顶点的边数，出度是从顶点指出的边数。

### 邻接矩阵

顶点数为 n，邻接矩阵用 n*n 大小矩阵表示图，行和列索引表示节点位置，矩阵元素表示边。

## 典型题目

图的遍历关键是要将已经遍历过的节点标记出来，避免重复遍历死循环或超时。

### 区域问题

图中不同的符号标记出来不同区域，遍历不同的区域，DFS 和 BFS 都可以。

**思路一、深度优先搜索 DFS**

1. 遍历图节点（通常是遍历二维数组）。
2. 对要遍历的指定区域，每个节点递归遍历上下左右节点，直到节点不再指定区域内。

```cpp
void dfs(int r, int c) {
    if (r < 0 || r >= rows || c < 0 || c >= cols) return;
    if (graph[r][c] != target) return; // 不在指定区域
    if (visited.count({r,c}));
    visited.insert({r,c}); // 标记已经访问过
    dfs(r+1, c);
    dfs(r-1, c);
    dfs(r, c+1);
    dfs(r, c-1);
};

for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
        if (graph[row][col] == target) {
            dfs(row, col);
            count++; // 区域个数
        }
    }
}
```

**思路二、广度优先搜索 BFS**

1. 遍历图节点
2. 对指定区域的节点加入队列中
3. 遍历队列中的节点，对节点的上下左右节点如果在指定区域就添加到队列中继续遍历。

```cpp
for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
        if (visited.count({row, col})) continue;
        if (graph[row][col] == target) {
            queue.push({row, col});
            visited.insert({row, col});
            while (!queue.empty()) {
                auto [r, c] = queue.front(); queue.pop();
                for (auto &next : dirs) {
                    int newR = r + next, newC = c + next; // 下一个节点
                    if (visited.count({newR, newC})) continue;
                    queue.push({newR, newC});
                    visited.insert({newR, newC});
                }
            }
        }
    }
}
```

### 拓扑排序

拓扑排序的数据必须是**无环有向图 DAG**。

**思路：**

1. 构建有向图的邻接表和入度表
2. 将所有入度为 0 的节点加入队列中，保存到结果。
3. 遍历队列中的节点，并将所有指向的下一节点的入度 - 1，此时入度为 0 的节点继续保存到队列和结果中。
4. 重复遍历队列操作，直到队列为空。

如果遍历完结果数组中的元素个数比有向图节点少，说明有向图中存在环。

典型题目：207、210 课程表

### 最短路径搜索

最短路径搜索使用 BFS 遍历。

**思路：**

1. 将搜索目标加入 unordered_set 中方便匹配。
2. 遍历的起始节点入队列。
3. 遍历队列的节点：
    1. 队列中节点如果是终点结束遍历
    2. 队列中节点依次遍历下一步节点，如果没有访问过且是可以走的路线，加入队列和已经访问的节点

    典型题目：909 蛇梯棋、433 最小基因变化、127 单词接龙。







