#include <iostream>
#include <algorithm>
#include <cassert>
using namespace std;

// 大域変数
const int T = 9; // 全ターン数
const int P = 4; // プレイヤー数
const int N = 6; // プログラミングの言語数
const int maxpt = 6;
const int minpt = 3;
const int wdays = 5;
const int hdays = 2;
int A[N]; // 言語の注目数
int priority[N];
/* 
   priority[0] 取りに行く言語1
   priority[1] 取りに行く言語2

   priority[2] 落とさない言語1
   priority[3] 落とさない言語2 できれば取りにいく
   priority[4] 落とさない言語3

   priority[5] 捨てる言語
 */

double K_top = 1.35; // トップになるための係数
double K_bottom = 1.3; // 最下位にならないための係数
bool is_defined_amari = false;
int amari = 0; // 余った時 

// ターンでの変数
int turn; // 現在のターン
bool isweekday; // 平日か？
int B[N][P]; // B[n][p] = プレイヤーpの言語nへの信者数
int R[N]; // 自分の本当の投票数
int W[N]; // 前の休日までの、明らかになっていない休日の布教回数(自分を除く)
int days; // 最大数
int L[wdays]; // 出力

void first_init() {
  // 初期化・入力
  int x;
  cin >> x;
  cin >> x;
  cin >> x;
  for (int i=0; i<N; i++) {
    cin >> A[i];
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
  amari = priority[3];
  return;
}

void turn_init() {
  cin >> turn;
  char D;
  cin >> D;
  isweekday = (D == 'W');
  if (isweekday) {
    days = wdays;
  } else {
    days = hdays;
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
  if (isweekday) {
    for (int i=0; i<N; i++) {
      int w;
      cin >> w;
      W[i] += w;
    }
  }
}

bool hantei(int lang, bool istop) {
  // // cerr << "days = " << days << endl;
  int m = B[lang][1];
  int K = K_top;
  for (int i=1; i<P; i++) {
    if (istop) {
      m = max(m, B[lang][i]);
      K = K_top;
    } else {
      m = min(m, B[lang][i]);
      K = K_bottom;
    }
  }
  return (R[lang] >= m + K * W[lang]);
}

bool isgood(int lang_priority) {
  int lang = priority[lang_priority];
  if (lang_priority <= 1) {
    return hantei(lang, true);
  } else {
    return hantei(lang, false);
  }
}

void define_amari() { // priority[2,3,4]から選ぶ
  int ret_lang = priority[2];
  int distance = 10000;
  for (int i=2; i<=4; i++) {
    int lang = priority[i];
    int temp = -10000;
    for (int j=1; j<P; j++) {
      temp = max(temp, B[lang][j]);
    }
    int tdist = temp + K_top*W[lang] - R[lang];
    if (tdist < distance) {
      distance = tdist;
      ret_lang = lang;
    }
  }
  amari = ret_lang;
  is_defined_amari = true;
}

void determine_L() {
  if (turn == 1) {
    for (int i=0; i<days; i++) {
      L[i] = priority[4-i];
    }
  } else if (turn == 2) {
    for (int i=0; i<days; i++) {
      L[i] = priority[0];
    }
  } else {
    int now_c = 0;
    int now_p = 0;
    // 戦略に基づき割り振る
    while (now_p < 5 && now_c < days) {
      if (!isgood(now_p)) {
	int lang = priority[now_p];
	L[now_c++] = lang;
	R[lang] += ((isweekday) ? 1 : 2);
      } else {
	now_p++;
      }
    }
    // 枠が余ったら
    while (now_c < days) {
      if (!is_defined_amari) {
	define_amari();
      }
      L[now_c++] = amari;
    }
  }
}

void turn_output() {
  determine_L();
  for (int i=0; i<days; i++) {
    cout << L[i];
    if (i != days-1) {
      cout << " ";
    }
    if(!isweekday) {
      W[L[i]]--;
    }
  }
  cout << endl;
}

int main() {
  cout << "READY" << endl;
  // ゲーム設定の入力
  first_init();
  // ターン情報の入力
  for (int t=1; t<=T; t++) {
    turn_init();
    turn_output();
  }
}
