#include <iostream>
#include "natural.h"
#include <time.h>
using namespace std;

int main() {

	cout << pow(natural(2), 65536) << endl;
	
/*
	natural num, den;
	clock_t t = clock();
	calc_e(num, den, 0, 1ull << 21);
	num *= pow(natural(10), 10000000);
	num /= den;
	cerr << clock() - t << endl;
	freopen("e_10000000.txt", "w", stdout);
	cout << num << endl;
*/
/*
	natural num, den, numc, denc;
	clock_t t = clock();
	calc_pi(num, den, numc, 1, 1ull << 20 | 1);
	sqrt_10005(numc, denc, 600000);
	num = 0xcf6371 * den - num;
	den *= 0x68380 * numc * pow(natural(10), 10000000);
	num *= denc;
	den /= num;
	cerr << clock() - t << endl;
	freopen("pi_10000000_new.txt", "w", stdout);
	cout << den << endl;
*/
/*
	clock_t t = clock();
	natural f = factorial(10000000);
	cerr << clock() - t << endl;
	freopen("factorial_10000000.txt", "w", stdout);
	cout << f << endl;
*/
}