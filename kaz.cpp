#include <iostream>
#include <algorithm>
#include <cassert>
using namespace std;

// 大域変数・定数
const int T = 9; // 全ターン数
const int P = 4; // プレイヤー(大名)数
const int N = 6; // 領主の数
const int remain[10] = {23, 23, 18, 14, 9, 5, 18, 14, 9, 5}; // 残り投票数
const int maxpt = 6;
const int minpt = 3;
const int noonpeople = 5;
const int nightpeople = 2;
int A[N]; // 領主の兵力
int total_score; // 兵力の合計値
int minimum_score; // 合格最低点
int priority[N];
int x_now=0, y_now=0; // 現在の戦略
int top_load, bottom_load;
int now_amari = 0;

// 戦略変数
double K_top[2] = {1.35, 1.35}; // トップになるための係数
double K_bottom[2] = {1.3, 1.3}; // 最下位にならないための係数
double C_top[2] = {1, 0}; // トップになるための定数
double C_bottom[2] = {0, 0}; // 最下位にならないための定数
double minimum_rate = 0.35; // 合格最低点/兵力の合計値

// ターンでの変数
int turn; // 現在のターン
bool isnoon; // 昼か？
int B[N][P]; // B[n][p] = プレイヤーpの領主nへの親密度
int R[N]; // 自分の本当の投票数
int W[N]; // 前の休日までの、明らかになっていない夜の交渉回数(自分を除く)
int people; // 出力数
int L[noonpeople]; // 出力
bool voted_tops = false; // そのターンに「取りに行く領主」に投票したか
bool voted_bottoms = false; // そのターンに「落とさない領主」に投票したか

void first_init() {
  // 初期化・入力
  int x;
  cin >> x;
  cin >> x;
  cin >> x;
  total_score = 0;
  for (int i=0; i<N; i++) {
    cin >> A[i];
    total_score += A[i];
  }
  fill(W, W+N, 0);
  // priorityの確定
  bool used[N];
  fill(used, used+N, false);
  // priority[2]の確定
  for (int i=maxpt; i>=minpt; i--) {
    for (int j=0; j<N; j++) {
      if (A[j] == i) {
	priority[2] = j;
	used[j] = true;
	goto EXIT_OF_PRIORITY_2;
      }
    }
  }
 EXIT_OF_PRIORITY_2:
  // priority[0][1]の確定
  int count = 0;
  for (int i=maxpt; i>=minpt; i--) {
    for (int j=N-1; j>=0; j--) {
      if (!used[j] && A[j] == i) {
	priority[count++] = j;
	used[j] = true;
	if (count == 2) {
	  goto EXIT_OF_PRIORITY_01;
	}
      }
    }
  }
 EXIT_OF_PRIORITY_01:
  // priority[3][4][5]の確定
  count = 3;
  for (int i=maxpt; i>=minpt; i--) {
    for (int j=N-1; j>=0; j--) {
      if (!used[j] && A[j] == i) {
	priority[count++] = j;
	used[j] = true;
	if (count == 5) {
	  goto EXIT_OF_PRIORITY_345;
	}
      }
    }
  }  
 EXIT_OF_PRIORITY_345:
  // amari = priority[3];
  return;
}

void turn_init() {
  cin >> turn;
  char D;
  cin >> D;
  isnoon = (D == 'D');
  if (isnoon) {
    people = noonpeople;
  } else {
    people = nightpeople;
  }
  for (int i=0; i<N; i++) {
    for (int j=0; j<P; j++) {
      cin >> B[i][j];
    }
  }
  for (int i=0; i<N; i++) {
    cin >> R[i];
  }
  if (turn == 6) {
    fill(W, W+P, 0);
  }
  if (isnoon) {
    for (int i=0; i<N; i++) {
      int w;
      cin >> w;
      W[i] += w;
    }
  }
}

int need_votes(int lord, bool istop) {
  int m = B[lord][1];
  double K = K_top[0];
  double Const = C_top[0];
  int be_af = ( (turn <= 5) ? 0 : 1 );
  for (int i=1; i<P; i++) {
    if (istop) {
      m = max(m, B[lord][i]);
      K = K_top[be_af];
      Const = C_top[be_af];
    } else {
      m = min(m, B[lord][i]);
      K = K_bottom[be_af];
      Const = C_bottom[be_af];
    }
  }
  return m + K * W[lord] + Const;
}

bool hantei(int lord, bool istop) {
  return R[lord] >= need_votes(lord, istop);
}


bool isgood(int lord_priority) {
  int lord = priority[lord_priority];
  if (lord_priority <= 1) {
    return hantei(lord, true);
  } else {
    return hantei(lord, false);
  }
}

void determine_minimum_score() {
  minimum_score = (int) (minimum_rate * total_score);
}

bool isvalid(int x, int y) { // xはtopに1が立っている。yはtopとbottomに1が立っている。xに1が立っているのにyに0が立っている場合はfalseを返す。そうでなければ有効なのでtrueを返す。
  for (int i=0; i<N; i++) {
    if ( (((x >> i) & 1) == 1) && (((y >> i) & 1) == 0) ) {
      return false;
    }
  }
  return true;
}

int score(int x) {
  int ret = 0;
  for (int i=0; i<N; i++) {
    if ((x >> i) & 1) {
      ret += A[i];
    }
  }
  return ret;
}

void determine_priority() {
  determine_minimum_score();
  int max_waste = -100000;
  int max_x = x_now;
  int max_y = y_now;
  for (int x=0; x< (1 << N); x++) {
    for (int y=0; y< (1 << N); y++) {
      if (isvalid(x, y) && score(x) > minimum_score) {
        // そもそも残りのターンで実現可能か？
        int n_vote = 0;
        for (int i=0; i<N; i++) {
          if ( (x >> i) & 1 ) {
            n_vote += max(need_votes(i, true) - R[i], 0);
          } else if ( (y >> i) & 1 ) {
            n_vote += max(need_votes(i, false) - R[i], 0);            
          } else {
            // 投票する必要はない
          }
        }
        if (n_vote > remain[turn]) continue;
        // 相手に捨てさせる票数の計算
        int waste = 0;
        for (int i=0; i<N; i++) {
          if ( (x >> i) & 1 ) {
            
          } else if ( (y >> i) & 1 ) {
            
          } else {

          }
        }
      }
    }
  }
}

void determine_L() {
  if (turn == 1) {
    for (int i=0; i<people; i++) {
      L[i] = priority[i];
    }
  } else if (turn == 2) {
    for (int i=0; i<people; i++) {
      L[i] = priority[i];
    }
  } else {
    int now_c = 0;
    int now_p = 0;
    // 戦略に基づき割り振る
    while (now_p < 5 && now_c < people) {
      if (!isgood(now_p)) {
	int lord = priority[now_p];
	L[now_c++] = lord;
	if (now_p <= 1) voted_tops = true;
	if (now_p >= 2) voted_bottoms = true;
	R[lord] += ((isnoon) ? 1 : 2);
      } else {
	now_p++;
      }
    }
    // 枠が余ったら
    while (now_c < people) {
      // cerr << "amari" << endl;
      L[now_c++] = priority[(now_amari++)%5];
    }
  }
}

void turn_output() {
  determine_L();
  sort(L, L+people);
  for (int i=0; i<people; i++) {
    cout << L[i];
    if (i != people-1) {
      cout << " ";
    }
    if(!isnoon) {
      W[L[i]]--;
    }
  }
  cout << endl;
}

void circle() {
  if (voted_tops) {
    swap(priority[0], priority[1]);
    voted_tops = false;
  }
  if (voted_bottoms) {
    swap(priority[2], priority[3]);
    swap(priority[3], priority[4]);
    voted_bottoms = false;
  }
}

int main() {
  cout << "READY" << endl;
  // ゲーム設定の入力
  first_init();
  // ターン情報
  for (int t=1; t<=T; t++) {
    turn_init();
    turn_output();
    circle();
  }
}
