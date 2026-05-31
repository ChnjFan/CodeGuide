#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

// -- core code template start
class Solution {
public:
    std::string addStrings(std::string num1, std::string num2) {
        std::string res;      // 保存结果
        int i = num1.size() - 1;  // 指向 num1 末尾
        int j = num2.size() - 1;  // 指向 num2 末尾
        int carry = 0;        // 进位

        // 从后往前，模拟竖式加法
        while (i >= 0 || j >= 0 || carry > 0) {
            int sum = carry;

            // 加上当前位数字
            if (i >= 0) sum += num1[i--] - '0';
            if (j >= 0) sum += num2[j--] - '0';

            carry = sum / 10;          // 新的进位
            res.push_back(sum % 10 + '0'); // 当前位结果
        }

        // 因为是从低位开始算的，最后要反转
        std::reverse(res.begin(), res.end());
        return res;
    }

    string gridPaths(int m, int n) {
        if (m == 0 || n == 0) return "0";
        if (m == 1 || n == 1) return "1";
        return addStrings(gridPaths(m-1, n), gridPaths(m, n-1));
    }

};
// -- core code template end

int main() {
    int m, n;
    while (cin >> m >> n) {
        string result = Solution().gridPaths(m, n);

        cout << result << endl;

    }
    return 0;
}