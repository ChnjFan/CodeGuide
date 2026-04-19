# 分治

**分治（Divide and Conquer）**，全称 “分而治之”，将大问题拆解成多个结构相同的小问题，分别解决后再将结果合并。

[TOC]

## 核心思想

分治必须满足三个条件：

1. 问题可拆分：大问题拆成规模更小、结构完全相同的子问题。
2. 子问题独立：子问题之间互不干扰、不重叠。
3. 可合并：子问题的解可以合并成原问题的解。

## 标准步骤

1. 划分：将原问题分为规模更小的子问题。
2. 解决：如果子问题足够小，直接求解，否则递归继续分治。
3. 合并：将所有子问题的解合并，得到原问题的解。
4. 结束条件：问题划分足够小，直接返回结果。

```cpp
void dfs(Problem *p) {
    if (isSmallProblem(p)) return;
    // 拆分子问题
    sub_problems = splite(p);
    // 分别解决子问题
    res1 = dfs(sub_problems[0]);
    res2 = dfs(sub_problems[1]);
    // 合并子问题解
    return merge(res1, res2);
}
```

## 典型题目

### 归并排序

链表排序问题，时间复杂度是 O(n logn)

1. 使用二分查找找到链表的中间节点，将链表分为两部分；
2. 分别对两部分链表进行排序；
3. 将排好序的两部分链表合并为同一个链表

```cpp
ListNode* sort(ListNode* root) {
    if (!root || !root->next) return root;// 空或者只有一个节点不需要排序
    // 快慢指针找中点
    ListNode *slow = root, *fast = root->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    ListNode *mid = slow->next;
    slow->next = nullptr;
    
    // 分别对两部分链表排序
    ListNode *left = sort(root);
    ListNode *right = sort(root);
    // 将两部分链表合并在一起
    return mearge(left, right);
}
```

