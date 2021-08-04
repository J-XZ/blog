#include <bits/stdc++.h>

using namespace std;

int input[6] = {4, 3, 2, 1, 6, 5};
struct node {
    int value;
    int index;
    node *next;
} nodes[] = {{0x014c, 1, nodes + 1},
             {0x00a8, 2, nodes + 2},
             {0x039c, 3, nodes + 3},
             {0x02b3, 4, nodes + 4},
             {0x01dd, 5, nodes + 5},
             {0x01bb, 6, nullptr}};
node *head = nodes;
node *new_array[6];

int bomb() {
    cout << "bomb" << endl;
    return 1;
};

int test() {
    /// 输入的6个数字都不能大于6，且unique
    for (int i = 0; i < 6; i++) {
        if (input[i] > 6)
            return bomb();
        for (int j = i + 1; j < 6; j++)
            if (input[i] == input[j])
                return bomb();
    }
    /// 输入的每个数字改成7-它自己
    for (int &i : input) i = 7 - i;
    /// 按照输入数组的顺序找到链表节点
    for (int i = 0; i < 6; i++)
        if (input[i] == 1)
            new_array[i] = head;
        else {
            node *p = head;
            for (int j = 1; j < input[i]; j++)
                p = p->next;
            new_array[i] = p;
        }
    /// 重新链接原链表
    head = new_array[0];
    for (int i = 0; i < 6; i++)
        new_array[i]->next = (i == 5 ? nullptr : new_array[i + 1]);
    /// 遍历新链表，查看是否满足递减
    for (node *p = head; p->next != nullptr; p = p->next)
        if (p->value < p->next->value)
            return bomb();
    return 0;
}

int main() {
    cout << test();
    return 0;
}
