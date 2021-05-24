// dither_dll.cpp : DLL 用にエクスポートされる関数を定義します。
//

#include "stdafx.h"
#include "dither_dll.h"


// ピクセルのデータを扱いやすくするための構造体
struct Pixel_RGBA {
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
};

// Bayer Matrix の生成
void BayerMatrix(double *M, int p) {
	if (p < 1) {
		M[0] = 0.0;
	}
	else {
		int n = 1 << p;
		int h = n >> 1;
		double* m = new double[h * h];
		BayerMatrix(m, p-1);
		

		// M の 左上
		for (int y = 0; y < n >> 1; y++) {
			for (int x = 0; x < n >> 1; x++) {
				M[y * h + x] = m[y * h + x];
			}
		}

		// M の 右上
		for (int y = 0; y < n >> 1; y++) {
			for (int x = 0; x < n >> 1; x++) {
				M[y * h + x + h] = m[y * h + x] + 2.0 / (n * n);
			}
		}

		// M の 左下
		for (int y = 0; y < n >> 1; y++) {
			for (int x = 0; x < n >> 1; x++) {
				M[(y + h) * h + x] = m[y * h + x] + 3.0 / (n * n);
			}
		}

		// M の 右下
		for (int y = 0; y < n >> 1; y++) {
			for (int x = 0; x < n >> 1; x++) {
				M[(y + h) * h + x + h] = m[y * h + x] + 1.0 / (n * n);
			}
		}

		delete[] m;
	}
}

static const unsigned char M[8 * 8] = {
	 0,48,12,60, 3,51,15,63,
	32,16,44,28,35,19,47,31,
	 8,56, 4,52,11,59, 7,55,
	40,24,36,20,43,27,39,23,
	 2,50,14,62, 1,49,13,61,
	34,18,46,30,33,17,45,29,
	10,58, 6,54, 9,57, 5,53,
	42,26,38,22,41,25,37,21 };

// Luaから呼び出す関数
int dither(lua_State *L) {
	// 引数の受け取り
	Pixel_RGBA *pixels = reinterpret_cast<Pixel_RGBA*>(lua_touserdata(L, 1));
	int w = static_cast<int>(lua_tointeger(L, 2));
	int h = static_cast<int>(lua_tointeger(L, 3));
	int p = static_cast<int>(lua_tointeger(L, 4));
	// double rm = lua_tonumber(L, 4);

	// BayerMatrix を用意
	// int n = 1 << p;
	// double* M = new double[n * n];
	// BayerMatrix(M, p);

	// forループで全ピクセルを処理
	// xが内側なのは横に操作した方がメモリの効率がよさそうだから
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = x + w * y;
			int peindex = (x % 8) + 8 * (y % 8);

			pixels[index].r = 4 * M[peindex] + 2 < pixels[index].r ? 0xff : 0x00 ;
			pixels[index].g = 4 * M[peindex] + 2 < pixels[index].g ? 0xff : 0x00 ;
			pixels[index].b = 4 * M[peindex] + 2 < pixels[index].b ? 0xff : 0x00 ;

			// pixels[index].r = 0xff * M[peindex];
			// pixels[index].g = 0xff * M[peindex];
			// pixels[index].b = 0xff * M[peindex];
		}
	}

	// delete[] M;

	// 今回はLua側へ値を返さないので0を返す
	return 0;
}

static luaL_Reg functions[] = {
	{"dither", dither},
	{nullptr, nullptr}
};

// これは、エクスポートされた関数の例です。
DITHERDLL_API int luaopen_OrderedDithering(lua_State *L)
{
	luaL_register(L, "OrderedDithering", functions);
    return 0;
}