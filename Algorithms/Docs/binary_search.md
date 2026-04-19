# 二分查找

二分查找是**基于有序数组、分治思想的高效查找算法**：每次将查找区间**一分为二**，比较中间值与目标，排除一半不可能的区间，重复直到找到或区间为空。

[TOC]

## 实现方式

针对数组中不同的区间，二分查找有三种不同的实现方式。

### 左闭右闭区间

查找数组中是否存在某个值，该值唯一存在，在 [left, right] 区间中查找。

```cpp
int binarySearch(vector<int>& arr, int target) {
    int left = 0, right = arr.size()-1;
    while (left <= right) {
		int mid = left + (right - left) / 2;
        if (arr[mid] == target) return mid;
        else if (arr[mid] < target) left = mid + 1;
        else right = mid - 1;
    }
    return -1;
}
```

### 左闭右开区间

查找数组中的目标值存在多个，找到最左侧的索引，或者找适合插入的索引位置。

```cpp
int binarySearch(vector<int>& arr, int target) {
    int left = 0, right = arr.size();
    while (left < right) {
        int mid = left + (right - left) / 2;
        if (arr[mid] < target) left = mid + 1;
        else right = mid;
    }
    return left;
}
```

### 左开右闭区间

查找数组中的目标值存在多个，找到最右侧的索引，或者找适合插入的索引位置。

```cpp
int binarySearch(vector<int>& arr, int target) {
    int left = 0, right = arr.size(), res = 0;
    while (left < right) {
        int mid = left + (right - left) / 2;
        if (arr[mid] <= target) {
            res = mid;
            left = mid + 1;
        }
        else right = mid;
    }
    return left;
}
```

