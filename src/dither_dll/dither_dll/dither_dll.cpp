﻿// dither_dll.cpp : DLL 用にエクスポートされる関数を定義します。
//

#include "stdafx.h"
#include "dither_dll.h"

using namespace std;

// ピクセルのデータを扱いやすくするための構造体
struct Pixel_RGBA {
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
};

struct Pixel_RGB {
	int b;
	int g;
	int r;
};

// Bayer 行列 ( 8 * 8 )
static const unsigned char M[8 * 8] = {
	 0,48,12,60, 3,51,15,63,
	32,16,44,28,35,19,47,31,
	 8,56, 4,52,11,59, 7,55,
	40,24,36,20,43,27,39,23,
	 2,50,14,62, 1,49,13,61,
	34,18,46,30,33,17,45,29,
	10,58, 6,54, 9,57, 5,53,
	42,26,38,22,41,25,37,21 };

// W3C標準pallet
static const unsigned char W3C[16][3] = {
	{0x00, 0x00, 0x00}, {0x80, 0x80, 0x80}, {0xc0, 0xc0, 0xc0}, {0xff, 0xff, 0xff},
	{0x00, 0x00, 0xff}, {0x00, 0x00, 0x80}, {0x00, 0x80, 0x80}, {0x00, 0x80, 0x00},
	{0x00, 0xff, 0x00}, {0x00, 0xff, 0xff}, {0xff, 0xff, 0x00}, {0xff, 0x00, 0x00},
	{0xff, 0x00, 0xff}, {0x80, 0x80, 0x00}, {0x80, 0x00, 0x80}, {0x80, 0x00, 0x00}
};

// rgb_sp[red][green][blue]
static int rgb_sp[16][16][16];

static int red_sp[16];
static int green_sp[16];
static int blue_sp[16];

static unsigned char pallet[256][3];

// ガンマ補正
double gamma_correction(double vl) {
	if (vl <= 0.0031308) {
		return 12.92 * vl;
	}
	else {
		return 1.055 * pow(vl, 1.0 / 2.4) - 0.055;
	}
}

// 逆ガンマ補正
double gamma_incorrection(double v) {
	if (v <= 0.0404499) {
		return v / 12.92;
	}
	else {
		return pow(v + 0.055, 2.4) / 1.1371189;
	}
}


// RGB -> 線形RGB
tuple<double, double, double> RGB2lRGB(unsigned char r, unsigned char g, unsigned char b) {
	double RGB[3];
	unsigned char rgb[3] = { r, g, b };

	for (int i = 0; i < 3; i++) {
		double v = rgb[i] / 255.0;
		if (v <= 0.0404499) {
			RGB[i] = v / 12.92;
		}
		else {
			RGB[i] = pow(v + 0.055, 2.4) / 1.1371189;
		}
	}

	return forward_as_tuple(RGB[0], RGB[1], RGB[2]);
}

// 線形RGB -> RGB
tuple<unsigned char, unsigned char, unsigned char> lRGB2RGB(double R, double G, double B) {
	double RGB[3] = {R, G, B};
	double rgb[3];
	unsigned char r, g, b;

	for (int i = 0; i < 3; i++) {
		double V = RGB[i];
		if (V <= 0.0031308) {
			rgb[i] = 12.92 * V;
		}
		else {
			rgb[i] = 1.055 * pow(V, 1.0/2.4) - 0.055;
		}
	}

	r = static_cast<unsigned char>(0xff * rgb[0]);
	g = static_cast<unsigned char>(0xff * rgb[1]);
	b = static_cast<unsigned char>(0xff * rgb[2]);

	return forward_as_tuple(r, g, b);
}

// 線形RGB -> XYZ
tuple<double, double, double> lRGB2XYZ(double R, double G, double B) {
	double X, Y, Z;

	X = 0.49000 * R + 0.31000 * G + 0.20000 * B;
	Y = 0.17697 * R + 0.81240 * G + 0.01063 * B;
	Z = 0.00000 * R + 0.01000 * G + 0.99000 * B;

	return forward_as_tuple(X / 0.17697, Y / 0.17697, Z / 0.17697);
}

// XYZ -> 線形RGB
tuple<double, double, double> XYZ2lRGB(double X, double Y, double Z) {
	double R, G, B;

	R =  0.41847    * X - 0.15866   * Y - 0.082835 * Z;
	G = -0.091169   * X + 0.25243   * Y + 0.015708 * Z;
	B =  0.00092090 * X - 0.0025498 * Y + 0.17860  * Z;

	return forward_as_tuple(R, G, B);
}

double Lab_f(double t) {
	return t > 216.0 / 24389.0 ? pow(t, 1.0/3.0) : t * 841.0 / 972.0 + 4.0/29.0;
}

// XYZ -> L*a*b*
tuple<double, double, double> XYZ2Lab(double X, double Y, double Z) {
	double L, a, b;

	L = 116.0 * Lab_f(Y / 100.0) - 16.0;
	a = 500.0 * (Lab_f(X / 95.0489) - Lab_f(Y / 100.0));
	b = 200.0 * (Lab_f(Y / 100.0) - Lab_f(Z / 108.8840));

	return forward_as_tuple(L, a, b);
}

double Lab_f_inv(double t) {
	return t > 6.0 / 29.0 ? t * t * t : 3 * 36.0 * (t - 4.0 / 29.0) / 841.0;
}

// L*a*b* -> XYZ
tuple<double, double, double> Lab2XYZ(double L, double a, double b) {
	double XYZ[3];

	XYZ[0] = 95.0489 * Lab_f_inv((L + 16.0) / 116.0  + a / 500.0);
	XYZ[1] = 100.0 * Lab_f_inv((L + 16.0) / 116.0);
	XYZ[2] = 108.8840 * Lab_f_inv((L + 16.0) / 116.0 - b / 200.0);

	return forward_as_tuple(XYZ[0], XYZ[1], XYZ[2]);
}

// RGB -> XYZ
tuple<double, double, double> RGB2XYZ(unsigned char r, unsigned char g, unsigned char b) {
	double X, Y, Z;

	tie(X, Y, Z) = RGB2lRGB(r, g, b);
	tie(X, Y, Z) = lRGB2XYZ(X, Y, Z);

	return forward_as_tuple(X, Y, Z);
}

// XYZ -> RGB
tuple<unsigned char, unsigned char, unsigned char> XYZ2RGB(double X, double Y, double Z) {
	double R, G, B;
	unsigned char r, g, b;

	tie(R, G, B) = XYZ2lRGB(X, Y, Z);
	tie(r, g, b) = lRGB2RGB(R, G, B);

	return forward_as_tuple(r, g, b);
}

// RGB -> L*a*b*
tuple<double, double, double> RGB2Lab(unsigned char r, unsigned char g, unsigned char b) {
	double L, A, B;

	tie(L, A, B) = RGB2lRGB(r, g, b);
	tie(L, A, B) = lRGB2XYZ(L, A, B);
	tie(L, A, B) = XYZ2Lab(L, A, B);

	return forward_as_tuple(L, A, B);
}

// L*a*b* -> RGB
tuple<unsigned char, unsigned char, unsigned char> Lab2RGB(double L, double A, double B) {
	double lR, lG, lB;
	unsigned char r, g, b;

	tie(lR, lG, lB) = Lab2XYZ(L, A, B);
	tie(lR, lG, lB) = XYZ2lRGB(lR, lG, lB);
	tie(r, g, b) = lRGB2RGB(lR, lG, lB);

	return forward_as_tuple(r, g, b);
}

double get_luminance(unsigned r, unsigned g, unsigned b) {
	return (r * 299 + g * 587 + b * 114) / (255.0 * 1000);
}

// CCIR 601 に基づく
double color_difference(unsigned r1, unsigned g1, unsigned b1, unsigned r2, unsigned g2, unsigned b2) {
	double luma1 = get_luminance(r1, g1, b1);
	double luma2 = get_luminance(r2, g2, b2);
	double lumadiff = luma1 - luma2;
	double diffR = (r1 - r2) / 255.0, diffG = (g1 - g2) / 255.0, diffB = (b1 - b2) / 255.0;
	return (diffR * diffR * 0.299 + diffG * diffG * 0.587 + diffB * diffB * 0.114) * 0.75 + lumadiff * lumadiff;
}

// Luaから呼び出す関数
// 8色によるディザリング
int simpleDither(lua_State *L) {
	// 引数の受け取り
	Pixel_RGBA *pixels = reinterpret_cast<Pixel_RGBA*>(lua_touserdata(L, 1));
	int w = static_cast<int>(lua_tointeger(L, 2));
	int h = static_cast<int>(lua_tointeger(L, 3));

	// forループで全ピクセルを処理
	// xが内側なのは横に操作した方がメモリの効率がよさそうだから
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = x + w * y;
			int peindex = (x % 8) + 8 * (y % 8);

			pixels[index].r = 4 * M[peindex] + 2 < pixels[index].r ? 0xff : 0x00 ;
			pixels[index].g = 4 * M[peindex] + 2 < pixels[index].g ? 0xff : 0x00 ;
			pixels[index].b = 4 * M[peindex] + 2 < pixels[index].b ? 0xff : 0x00 ;
		}
	}

	// 今回はLua側へ値を返さないので0を返す
	return 0;
}

// メディアンカットによる減色
void medianCut(unsigned int N) {
	int rm, rM, gm, gM, bm, bM, pixel_num = 0;
	queue<int> q;
	
	// rm, rM の決定
	for (int r = 0x0; r <= 0xf; r++) {
		if (red_sp[r] > 0) {
			rm = r;
			break;
		}
	}
	for (int r = 0xf; r >= 0x0; r--) {
		if (red_sp[r] > 0) {
			rM = r;
			break;
		}
	}

	// gm, gM の決定
	for (int g = 0x0; g <= 0xf; g++) {
		if (green_sp[g] > 0) {
			gm = g;
			break;
		}
	}
	for (int g = 0xf; g >= 0x0; g--) {
		if (green_sp[g] > 0) {
			gM = g;
			break;
		}
	}

	// bm, bM の決定
	for (int b = 0x0; b <= 0xf; b++) {
		if (blue_sp[b] > 0) {
			bm = b;
			break;
		}
	}
	for (int b = 0xf; b >= 0x0; b--) {
		if (blue_sp[b] > 0) {
			bM = b;
			break;
		}
	}

	// pixel_num の決定
	for (int r = 0x0; r <= 0xf; r++) {
		pixel_num += red_sp[r];
	}

	q.push(rm); q.push(rM); q.push(gm); q.push(gM); q.push(bm); q.push(bM); q.push(pixel_num);
	
	// 分割数が n を越えるまで分割
	while (q.size() < 7 * N) {
		rm = q.front(); q.pop(); rM = q.front(); q.pop();
		gm = q.front(); q.pop(); gM = q.front(); q.pop();
		bm = q.front(); q.pop(); bM = q.front(); q.pop();
		pixel_num = q.front(); q.pop();

		// r が最長のとき
		if (rM - rm >= 1.2 * (gM - gm) && rM - rm >= 1.2 * (bM - bm)) {
			unsigned char rmed, rMed;
			int c = 0, C = 0;

			for (int r = rm; r <= rM; r++) {
				for (int g = gm; g <= gM; g++) {
					for (int b = bm; b <= bM; b++) {
						c += rgb_sp[r][g][b];
					}
				}
				if (c >= pixel_num / 2) {
					rmed = static_cast<unsigned char>(r);
					break;
				}
			}
			for (int r = rM; r >= rm; r--) {
				for (int g = gM; g >= gm; g--) {
					for (int b = bM; b >= bm; b--) {
						C += rgb_sp[r][g][b];
					}
				}
				if (C >= pixel_num / 2) {
					rMed = static_cast<unsigned char>(r);
					break;
				}
			}

			q.push(rm); q.push(rmed); q.push(gm); q.push(gM); q.push(bm); q.push(bM); q.push(c);
			q.push(rMed); q.push(rM); q.push(gm); q.push(gM); q.push(bm); q.push(bM); q.push(C);
		}
		// g が最長のとき
		else if (gM - gm >= bM - bm) {
			unsigned char gmed, gMed;
			int c = 0, C = 0;

			for (int g = gm; g <= gM; g++) {
				for (int b = bm; b <= bM; b++) {
					for (int r = rm; r <= rM; r++) {
						c += rgb_sp[r][g][b];
					}
				}
				if (c >= pixel_num / 2) {
					gmed = static_cast<unsigned char>(g);
					break;
				}
			}
			for (int g = gM; g >= gm; g--) {
				for (int b = bM; b >= bm; b--) {
					for (int r = rM; r >= rm; r--) {
						C += rgb_sp[r][g][b];
					}
				}
				if (C >= pixel_num / 2) {
					gMed = static_cast<unsigned char>(g);
					break;
				}
			}

			q.push(rm); q.push(rM); q.push(gm); q.push(gmed); q.push(bm); q.push(bM); q.push(c);
			q.push(rm); q.push(rM); q.push(gMed); q.push(gM); q.push(bm); q.push(bM); q.push(C);
		}
		// b が最長の時
		else {
			unsigned char bmed, bMed;
			int c = 0, C = 0;

			for (int b = bm; b <= bM; b++) {
				for (int r = rm; r <= rM; r++) {
					for (int g = gm; g <= gM; g++) {
						c += rgb_sp[r][g][b];
					}
				}
				if (c >= pixel_num / 2) {
					bmed = static_cast<unsigned char>(b);
					break;
				}
			}
			for (int b = bM; b >= bm; b--) {
				for (int r = rM; r >= rm; r--) {
					for (int g = gM; g >= gm; g--) {
						C += rgb_sp[r][g][b];
					}
				}
				if (C >= pixel_num / 2) {
					bMed = static_cast<unsigned char>(b);
					break;
				}
			}

			q.push(rm); q.push(rM); q.push(gm); q.push(gM); q.push(bm); q.push(bmed); q.push(c);
			q.push(rm); q.push(rM); q.push(gm); q.push(gM); q.push(bMed); q.push(bM); q.push(C);
		}
	}

	// パレットを作る
	for (unsigned int n = 0; n < N; n++) {
		double X = 0, Y = 0, Z = 0;
		unsigned char ToReturn_rgb[3];

		rm = q.front(); q.pop(); rM = q.front(); q.pop();
		gm = q.front(); q.pop(); gM = q.front(); q.pop();
		bm = q.front(); q.pop(); bM = q.front(); q.pop();
		pixel_num = q.front();  q.pop();
		
		if (pixel_num == 0) continue;

		for (int r = rm; r <= rM; r++) {
			for (int g = gm; g <= gM; g++) {
				for (int b = bm; b <= bM; b++) {
					double x, y, z;
					tie(x, y, z) = lRGB2XYZ(r / 15.0, g / 15.0, b / 15.0);
					X += x * rgb_sp[r][g][b];
					Y += y * rgb_sp[r][g][b];
					Z += z * rgb_sp[r][g][b];
				}
			}
		}

		tie(ToReturn_rgb[0], ToReturn_rgb[1], ToReturn_rgb[2]) = XYZ2RGB(X / pixel_num, Y / pixel_num, Z / pixel_num);
		
		pallet[n][0] = static_cast<unsigned char>(ToReturn_rgb[0]);
		pallet[n][1] = static_cast<unsigned char>(ToReturn_rgb[1]);
		pallet[n][2] = static_cast<unsigned char>(ToReturn_rgb[2]);
	}
}

// pallet で一番近い色を選ぶ
tuple<unsigned char, unsigned char, unsigned char> BestColor(unsigned char r, unsigned char g, unsigned char b, int N) {
	unsigned char best_rgb[3] = { pallet[0][0], pallet[0][1], pallet[0][2] };
	double best_dist = 256.0;

	for (int n = 1; n < N; n++) {
		double current_dist;

		current_dist = color_difference(r, g, b, pallet[n][1], pallet[n][2], pallet[n][3]);

		if (current_dist < best_dist) {
			best_rgb[0] = pallet[n][0];
			best_rgb[1] = pallet[n][1];
			best_rgb[2] = pallet[n][2];
			best_dist = current_dist;
		}
	}

	return forward_as_tuple(best_rgb[0], best_rgb[1], best_rgb[2]);
}

// W3Cで一番近い色を選ぶ
tuple<unsigned char, unsigned char, unsigned char> Color2W3C(unsigned char r, unsigned char g, unsigned char b) {
	unsigned char best_rgb[3] = { pallet[0][0], pallet[0][1], pallet[0][2] };
	double best_dist = 256.0;

	for (int n = 1; n < 16; n++) {
		double current_dist;

		current_dist = color_difference(r, g, b, W3C[n][1], W3C[n][2], W3C[n][3]);

		if (current_dist < best_dist) {
			best_rgb[0] = W3C[n][0];
			best_rgb[1] = W3C[n][1];
			best_rgb[2] = W3C[n][2];
			best_dist = current_dist;
		}
	}

	return forward_as_tuple(best_rgb[0], best_rgb[1], best_rgb[2]);
}

// 減色スクリプト
int degreaseColor(lua_State *L) {
	// 引数の受け取り
	Pixel_RGBA *pixels = reinterpret_cast<Pixel_RGBA*>(lua_touserdata(L, 1));
	int w = static_cast<int>(lua_tointeger(L, 2));
	int h = static_cast<int>(lua_tointeger(L, 3));
	int N = static_cast<int>(lua_tointeger(L, 4));

	// rgb_sp を設定
	for (int i = 0; i < 16; i++) {
		red_sp[i] = 0;
		green_sp[i] = 0;
		blue_sp[i] = 0;
	}
	for (int R = 0; R < 16; R++) {
		for (int G = 0; G < 16; G++) {
			for (int B = 0; B < 16; B++) {
				rgb_sp[R][G][B] = 0;
			}
		}
	}
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = x + w * y;
			unsigned char R, G, B;
			double dR, dG, dB;

			tie(dR, dG, dB) = RGB2lRGB(pixels[index].r, pixels[index].g, pixels[index].b);
			R = static_cast<unsigned char>(15.4 * dR);
			G = static_cast<unsigned char>(15.4 * dG);
			B = static_cast<unsigned char>(15.4 * dB);

			rgb_sp[R][G][B]++;
		}
	}

	medianCut(N);

	// forループで全ピクセルを処理
	// xが内側なのは横に操作した方がメモリの効率がよさそうだから
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = x + w * y;

			tie(pixels[index].r, pixels[index].g, pixels[index].b) = BestColor(pixels[index].r, pixels[index].g, pixels[index].b, N);
		}
	}

	// 今回はLua側へ値を返さないので0を返す
	return 0;
}

// W3C パレットに減色
int degrease2W3C(lua_State *L) {
	// 引数の受け取り
	Pixel_RGBA *pixels = reinterpret_cast<Pixel_RGBA*>(lua_touserdata(L, 1));
	int w = static_cast<int>(lua_tointeger(L, 2));
	int h = static_cast<int>(lua_tointeger(L, 3));
	// double rm = lua_tonumber(L, 4);

	// forループで全ピクセルを処理
	// xが内側なのは横に操作した方がメモリの効率がよさそうだから
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = x + w * y;

			tie(pixels[index].r, pixels[index].g, pixels[index].b) = Color2W3C(pixels[index].r, pixels[index].g, pixels[index].b);
		}
	}

	// 今回はLua側へ値を返さないので0を返す
	return 0;
}

// ディザ合成
int ditherComposer(lua_State *L) {
	// 引数の受け取り
	Pixel_RGBA *pixels = reinterpret_cast<Pixel_RGBA*>(lua_touserdata(L, 1));
	int w = static_cast<int>(lua_tointeger(L, 2));
	int h = static_cast<int>(lua_tointeger(L, 3));
	double alpha = lua_tonumber(L, 4) / 100.0;

	// forループで全ピクセルを処理
	// xが内側なのは横に操作した方がメモリの効率がよさそうだから
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = x + w * y;
			int peindex = (x % 8) + 8 * (y % 8);

			pixels[index].a = (1 - alpha) * pixels[index].a > 4 * M[peindex] + 2 ? 0xff : 0x00;
		}
	}

	// 今回はLua側へ値を返さないので0を返す
	return 0;
}

// パターンディザ
int patternDither(lua_State *L) {
	// 引数の受け取り
	Pixel_RGBA *pixels = reinterpret_cast<Pixel_RGBA*>(lua_touserdata(L, 1));
	int w = static_cast<int>(lua_tointeger(L, 2));
	int h = static_cast<int>(lua_tointeger(L, 3));
	double threshold = lua_tonumber(L, 4) / 100.0;
	int N = static_cast<int>(lua_tointeger(L, 5));

	// Median cut によりパレットを生成
	// rgb_sp を初期化
	for (int i = 0; i < 16; i++) {
		red_sp[i] = 0;
		green_sp[i] = 0;
		blue_sp[i] = 0;
	}
	for (int R = 0; R < 16; R++) {
		for (int G = 0; G < 16; G++) {
			for (int B = 0; B < 16; B++) {
				rgb_sp[R][G][B] = 0;
			}
		}
	}
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = x + w * y;
			unsigned char R, G, B;
			double dR, dG, dB;

			if (pixels[index].a == 0) break;

			tie(dR, dG, dB) = RGB2lRGB(pixels[index].r, pixels[index].g, pixels[index].b);

			R = static_cast<unsigned char>(15.4 * dR);
			G = static_cast<unsigned char>(15.4 * dG);
			B = static_cast<unsigned char>(15.4 * dB);

			rgb_sp[R][G][B]++;
			red_sp[R]++;
			green_sp[G]++;
			blue_sp[B]++;
		}
	}

	medianCut(N);

	// forループで全ピクセルを処理
	// xが内側なのは横に操作した方がメモリの効率がよさそうだから
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			// 色候補のリセット
			int index = x + w * y;
			int peindex = (x % 8) + 8 * (y % 8);
			Pixel_RGB color_error = {0, 0, 0};
			vector<Pixel_RGBA> candidate_list(0);
			// 色候補を生成
			while (candidate_list.size() < 8 * 8) {
				Pixel_RGB attempt = { 0, 0, 0 };
				attempt.r = static_cast<int>(1.0 * pixels[index].r - color_error.r * threshold);
				attempt.g = static_cast<int>(1.0 * pixels[index].g - color_error.g * threshold);
				attempt.b = static_cast<int>(1.0 * pixels[index].b - color_error.b * threshold);
				
				// Bumping
				attempt.r = attempt.r > 0x00 ? attempt.r : 0x00;
				attempt.r = attempt.r < 0xff ? attempt.r : 0xff;
				attempt.g = attempt.g > 0x00 ? attempt.g : 0x00;
				attempt.g = attempt.g < 0xff ? attempt.g : 0xff;
				attempt.b = attempt.b > 0x00 ? attempt.b : 0x00;
				attempt.b = attempt.b < 0xff ? attempt.b : 0xff;

				Pixel_RGBA candidate = { 0 ,0 ,0, 0 };
				tie(candidate.r, candidate.g, candidate.b) = BestColor(attempt.r, attempt.g, attempt.b, N);
				candidate_list.push_back(candidate);
				color_error.r = candidate.r - pixels[index].r;
				color_error.g = candidate.g - pixels[index].g;
				color_error.b = candidate.b - pixels[index].b;
			}
			// luma を初期化
			vector<pair<double, int>> luma_list(8 * 8);
			for (int i = 0; i < 8 * 8; i++) {
				double luma = get_luminance(candidate_list[i].r, candidate_list[i].g, candidate_list[i].b);
				luma_list[i] = make_pair(luma, i);
			}
			// luma 基準で candidate_list をソート
			sort(luma_list.begin(), luma_list.end());
			// 色候補で塗る
			int candidate_index = luma_list[M[peindex]].second;
			pixels[index].r = candidate_list[candidate_index].r;
			pixels[index].g = candidate_list[candidate_index].g;
			pixels[index].b = candidate_list[candidate_index].b;
		}
	}

	// 今回はLua側へ値を返さないので0を返す
	return 0;
}


static luaL_Reg functions[] = {
	{"simpleDither", simpleDither},
	{"degreaseColor", degreaseColor},
	{"w3c_degreaseColor", degrease2W3C},
	{"ditherComposer", ditherComposer},
	{"patternDither", patternDither},
	{nullptr, nullptr}
};

// これは、エクスポートされた関数の例です。
DITHERDLL_API int luaopen_OrderedDithering(lua_State *L)
{
	luaL_register(L, "OrderedDithering", functions);
    return 0;
}