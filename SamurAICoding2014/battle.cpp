// このコードはこの前のcode runner 2014に出た時のサンプルコードを改造したものです。queryのところの処理はブラックボックスです。

#include <string>
#include <random>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <typeinfo>
#include <tuple>
#include <unistd.h>
using namespace std;

string query(string str) {
  FILE *f = popen(str.c_str(), "r");
  if (f == NULL) {
    perror("error!");
  }
  char buf[1024];
  string res;
  while (!feof(f)) {
    if (fgets(buf, 1024, f) == NULL) break;
    res += (string)(buf);
  }
  pclose(f);
  return res;
}

// AI Commands: ["./0.out","./1.out","./2.out","./3.out"]
// AI Commands: ["./0.out","./1.out","./2.out","./3.out"]

string battle_str(int a, int b, int c, int d) {
  return "./execute_on_cui.sh -a ./"
    + to_string(a) + ".out -a ./"
    + to_string(b) + ".out -a ./"
    + to_string(c) + ".out -a ./"
    + to_string(d) + ".out";
}

int who_winner(string str) {
  int x = str.find("Winner");
  int res = -1;
  if (x < str.size()) {
    res = str[x+8] - '0';
  }
  if (res < 0 || res > 3) res = -1;
  return res;
}

int main() {
  int times = 0;
  const int C = 16;
  int win[C];
  int cast[4];
  fill(win, win+C, 0);
  while (true) {
    for (auto i=0; i<C; i++) {
      cast[0] = i;
      for (auto j=i+1; j<C; j++) {
        cast[1] = j;
        for (auto k=j+1; k<C; k++) {
          cast[2] = k;
          for (auto l=k+1; l<C; l++) {
            cast[3] = l;
            string res = query(battle_str(i, j, k, l));
            int winner = who_winner(res);
            if (winner != -1) {
              cerr << cast[winner] << endl;
              win[cast[winner]]++;
          }
          }
        }
      }
    }
    cout << "battle result of " << times++ << endl; 
    for (auto i=0; i<C; i++) {
      cout << "Player " << i << " : " << win[i] << endl;
    }
  }
}
