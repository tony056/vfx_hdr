#ifndef GIL_COLORSPACE_H
#define GIL_COLORSPACE_H

/* 
 * reference: 
 * 	http://en.wikipedia.org/wiki/Color_space
 * 	RGB: http://en.wikipedia.org/wiki/SRGB
 * 	CYMK: http://en.wikipedia.org/wiki/CMYK
 *  YUV: http://en.wikipedia.org/wiki/YUV
 *  YCbCr: http://en.wikipedia.org/wiki/YCbCr
 *  HSV: http://en.wikipedia.org/wiki/HSV_color_space
 *
 *  Conversion: 
 *    http://cmlab.csie.org/~tzhuan/www/resources/OpenCV/opencvref_cv.htm#cv_imgproc_filters
 *    http://www.easyrgb.com/math.php
 *
 */

#include <algorithm>
#include <cassert>
#include <complex>
#include <iostream>

#include "../gil.h"

namespace gil {

	template<typename T> struct RgbToGray;
	template<typename T, size_t C> 
	struct RgbToGray< Color<T, C> >
	{
		typedef T To;
		typedef Color<T, C> From;
		const T operator()(const From &from) const
		{
			// OpenCV:
			// RGB[A]->Gray: Y<-0.299*R + 0.587*G + 0.114*B

			return static_cast<T>(
				0.299*from[0] + 0.587*from[1] + 0.114*from[2]
			);
		}
	};
	template<typename T> struct RgbToGray< Color<T, 2> >;

	template<typename T>
	struct RgbToXyz {
		typedef T To;
		typedef T From;
		const To operator()(const From &from) const 
		{
			// OpenCV:
			// |X|    |0.412453  0.357580  0.180423| |R|
			// |Y| <- |0.212671  0.715160  0.072169|*|G|
			// |Z|    |0.019334  0.119193  0.950227| |B|

			return To(
				0.412453*from[0] + 0.357580*from[1] + 0.180423*from[2],
				0.212671*from[0] + 0.715160*from[1] + 0.072169*from[2],
				0.019334*from[0] + 0.119193*from[1] + 0.950227*from[2]
			);
		}
	};
	template<typename T> struct RgbToXyz< Color<T, 2> >;

	template<typename T>
	struct XyzToRgb {
		typedef T To;
		typedef T From;
		const To operator()(const From &from) const 
		{
			// OpenCV:
			// |R|    | 3.240479  -1.53715  -0.498535| |X|
			// |G| <- |-0.969256   1.875991  0.041556|*|Y|
			// |B|    | 0.055648  -0.204043  1.057311| |Z|

			return To(
				 3.240479*from[0] +  -1.53715*from[1] + -0.498535*from[2],
				-0.969256*from[0] +  1.875991*from[1] +  0.041556*from[2],
				 0.055648*from[0] + -0.204043*from[1] +  1.057311*from[2]
			);
		}
	};
	template<typename T> struct XyzToRgb< Color<T, 2> >;

	template<typename T>
	struct RgbToYCrCb {
		typedef T To;
		typedef T From;

		typedef typename ColorTrait<T>::BaseType value_type;

		const To operator()(const From &from) const 
		{
			// OpenCV:
			// Y <- 0.299*R + 0.587*G + 0.114*B
			// Cr <- (R-Y)*0.713 + delta
			// Cb <- (B-Y)*0.564 + delta
			//
			//		  / 128 for 8-bit images,
			// delta =  32768 for 16-bit images
			//		  \ 0.5 for floating-point images

			const value_type &R = from[0];
			const value_type &G = from[1];
			const value_type &B = from[2];
			const value_type delta = TypeTrait<value_type>::opaque() / 2;
                        //printf("%g \n",delta);
			value_type Y = RgbToGray<From>()(from);
			value_type Cr = static_cast<value_type>( (R-Y) * 0.713 + delta );
			value_type Cb = static_cast<value_type>( (B-Y) * 0.564 + delta );

			return To(Y, Cr, Cb);
		}
	};
	template<typename T> struct RgbToYCrCb< Color<T, 2> >;
	
	template<typename T>
	struct YCrCbToRgb {
		typedef T To;
		typedef T From;

		typedef typename ColorTrait<T>::BaseType value_type;

		const To operator()(const From &from) const 
		{
			// OpenCV:
			// R <- Y + 1.403*(Cr - delta)
			// G <- Y - 0.344*(Cr - delta) - 0.714*(Cb - delta)
			//   XXX must be G <- Y - 0.714*(Cb - delta) - 0.344*(Cr - delta)
			// B <- Y + 1.773*(Cb - delta),
			//
			//		  / 128 for 8-bit images,
			// delta =  32768 for 16-bit images
			//		  \ 0.5 for floating-point images

			const value_type &Y = from[0];
			const value_type &Cr = from[1];
			const value_type &Cb = from[2];

			const value_type delta = TypeTrait<value_type>::opaque() / 2;

			value_type R = static_cast<value_type>( Y + 1.403 * (Cr-delta) );
			value_type G = static_cast<value_type>( Y - 0.714* (Cr-delta) - 0.344 * (Cb-delta) );
			value_type B = static_cast<value_type>( Y + 1.773 * (Cb-delta) );

			return To(R, G, B);
		}
	};
	template<typename T> struct YCrCbToRgb< Color<T, 2> >;
	
	template<typename T>
	struct RgbToHsv {

		typedef T To;
		typedef T From;

		typedef typename ColorTrait<T>::BaseType value_type;
		typedef typename TypeTrait<value_type>::ExtendedType tmp_type;
		typedef typename ColorTrait<T>::ExtendedColor tmp_color;

		const To operator()(const From &from) const 
		{
			// OpenCV: 
			// R, G and B are converted to floating-point format and 
			// scaled to fit 0..1 range
			//
			// V <- max(R,G,B)
			// S <- (V-min(R,G,B))/V if V != 0, 0 otherwise
			//
			//	   / (G - B)*60/S,		if V=R
			// H =   180+(B - R)*60/S,	if V=G XXX: 180 should be 120!
			//	   \ 240+(R - G)*60/S,	if V=B
			//
			// if H<0 then H<-H+360
			//
			// On output 0 <= V <= 1, 0 <= S <= 1, 0 <= H <= 360.
			// The values are then converted to the destination data type:
			// 8-bit images:
			//   V <- V*255, S <- S*255, H <- H/2 (to fit to 0..255)
			// 16-bit images (currently not supported):
			//   V <- V*65535, S <- S*65535, H <- H
			// 32-bit images:
			//   H, S, V are left as is

			tmp_color tmp = DefaultConverter<tmp_color, T>()(from);
			const tmp_type &R = tmp[0];
			const tmp_type &G = tmp[1];
			const tmp_type &B = tmp[2];

			tmp_type V = max( max(R, G), max(G, B) );
			tmp_type S = V ? ( (V-min( min(R, G), min(G, B) ) ) / V ) : 0;
			tmp_type H;

			if (V == R)
				H = (G-B) * 60 / S;
			else if (V == G)
				H = 120 + (B-R) * 60 / S;
			else
				H = 240 + (R-G) * 60 / S;

			if (H < 0) H += 360;

			return To(
				static_cast<value_type>(H),
				static_cast<value_type>(S),
				static_cast<value_type>(V)
			);
		}
	};
	template<typename T> struct RgbToHsv< Color<T, 2> >;
	
	template<typename T>
	struct HsvToRgb {
		typedef T To;
		typedef T From;

		const To operator()(const From &from) const 
		{
			// TODO
			assert(0);
		}
	};
	template<typename T> struct HsvToRgb< Color<T, 2> >;
	
	template<typename T>
	struct RgbToHsl {
		typedef T To;
		typedef T From;

		typedef typename ColorTrait<T>::BaseType value_type;
		typedef typename TypeTrait<value_type>::ExtendedType tmp_type;
		typedef typename ColorTrait<T>::ExtendedColor tmp_color;

		const To operator()(const From &from) const 
		{
			// OpenCV:
			// R, G and B are converted to floating-point format and 
			// scaled to fit 0..1 range
			//
			// Vmax <- max(R,G,B)
			// Vmin <- min(R,G,B)
			//
			// L <- (Vmax + Vmin)/2
			//
			// S <- (Vmax - Vmin)/(Vmax + Vmin)			if L < 0.5
			//    \ (Vmax - Vmin)/(2 - (Vmax + Vmin))	if L >= 0.5
			//
			//	   / (G - B)*60/S,		if Vmax=R
			// H <-  180+(B - R)*60/S,	if Vmax=G XXX 180 must be 120!
			//	   \ 240+(R - G)*60/S,	if Vmax=B
			//
			// if H<0 then H<-H+360
			//
			// On output 0 <= L <= 1, 0 <= S <= 1, 0 <= H <= 360.
			// The values are then converted to the destination data type:
			// 8-bit images:
			//   L <- L*255, S <- S*255, H <- H/2
			// 16-bit images (currently not supported):
			//   L <- L*65535, S <- S*65535, H <- H
			// 32-bit images:
			//   H, L, S are left as is

			tmp_color tmp = DefaultConverter<tmp_color, T>()(from);
			const tmp_type &R = tmp[0];
			const tmp_type &G = tmp[1];
			const tmp_type &B = tmp[2];

			tmp_type Vmax = max( max(R, G), max(G, B) );
			tmp_type Vmin = min( min(R, G), min(G, B) );

			tmp_type L = (Vmax+Vmin) / 2;
			tmp_type S = (L < 0.5) ? 
				(Vmax-Vmin) / (Vmax+Vmin) : 
				(Vmax-Vmin) / ( 2 - (Vmax+Vmin) );
			tmp_type H;

			if (Vmax == R)
				H = (G-B) * 60 / S;
			else if (Vmax == G)
				H = 120 + (B-R) * 60 / S;
			else
				H = 240 + (R-G) * 60 / S;

			if (H < 0) H += 360;

			return To(
				static_cast<value_type>(H),
				static_cast<value_type>(S),
				static_cast<value_type>(L)
			);
		}
	};
	template<typename T> struct RgbToHsl< Color<T, 2> >;

	template<typename T>
	struct HslToRgb {
		typedef T To;
		typedef T From;
		
		const To operator()(const From &from) const 
		{
			// TODO
			assert(0);
		}
	};
	template<typename T> struct HslToRgb< Color<T, 2> >;
	
	template<typename T>
	struct RgbToLab {
		typedef T To;
		typedef T From;
		typedef typename ColorTrait<T>::BaseType value_type;
		typedef typename TypeTrait<value_type>::ExtendedType tmp_type;
		typedef typename ColorTrait<T>::ExtendedColor tmp_color;;

		const To operator()(const From &from) const 
		{
			// R, G and B are converted to floating-point format and
			// scaled to fit 0..1 range
			//
			// // convert R,G,B to CIE XYZ
			// |X|    |0.412453  0.357580  0.180423| |R|
			// |Y| <- |0.212671  0.715160  0.072169|*|G|
			// |Z|    |0.019334  0.119193  0.950227| |B|
			//
			// X <- X/Xn, where Xn = 0.950456
			// Z <- Z/Zn, where Zn = 1.088754
			//
			// L <- 116*Y1/3	for Y>0.008856
			// L <- 903.3*Y		for Y<=0.008856
			//
			// a <- 500*(f(X)-f(Y)) + delta
			// b <- 200*(f(Y)-f(Z)) + delta
			// where f(t)=t1/3				for t>0.008856
			//       f(t)=7.787*t+16/116	for t<=0.008856
			//
			// where delta = 128 for 8-bit images,
			//                 0 for floating-point images
			//
			// On output 0 <= L <= 100, -127 <= a <= 127, -127 <= b <= 127
			// The values are then converted to the destination data type:
			// 8-bit images:
			//   L <- L*255/100, a <- a + 128, b <- b + 128
			// 16-bit images are currently not supported
			// 32-bit images:
			//   L, a, b are left as is

			tmp_color tmp = 
				RgbToXyz<tmp_color>()( DefaultConverter<tmp_color, T>()(from) );

			const tmp_type &X = tmp[0];
			const tmp_type &Y = tmp[1];
			const tmp_type &Z = tmp[2];

			X /= 0.950456;
			Z /= 1.088754;

			const tmp_type delta = 0;
			tmp_type L = (Y > 0.008856) ? 
				116 * std::pow(Y, static_cast<tmp_type>(1./3)) : 
				903.3 * Y;
			tmp_type a = 500 * (f(X)-f(Y)) + delta;
			tmp_type b = 200 * (f(Y)-f(Z)) + delta;

			return To(
				static_cast<value_type>(L),
				static_cast<value_type>(a),
				static_cast<value_type>(b)
			);
		}

		private:
			inline static tmp_type f(tmp_type t) 
			{
				// where f(t)=t1/3				for t>0.008856
				//       f(t)=7.787*t+16/116	for t<=0.008856
				return (t > 0.008856) ? 
					std::pow(t, static_cast<tmp_type>(1/3.)) :
					7.787*t + 16./116;
			}
	};
	template<typename T> struct RgbToLab< Color<T, 2> >;
	
	template<typename T>
	struct LabToRgb {
		typedef T To;
		typedef T From;
		const To operator()(const From &from) const
		{
			// TODO
			assert(0);
		}
	};
	template<typename T> struct LabToRgb< Color<T, 2> >;
	
	template<typename T>
	struct RgbToLuv {
		typedef T To;
		typedef T From;

		typedef typename ColorTrait<T>::BaseType value_type;
		typedef typename TypeTrait<value_type>::ExtendedType tmp_type;
		typedef typename ColorTrait<T>::ExtendedColor tmp_color;;

		const To operator()(const From &from) const 
		{
			// R, G and B are converted to floating-point format and 
			// scaled to fit 0..1 range
			//
			// // convert R,G,B to CIE XYZ
			// |X|    |0.412453  0.357580  0.180423| |R|
			// |Y| <- |0.212671  0.715160  0.072169|*|G|
			// |Z|    |0.019334  0.119193  0.950227| |B|
			//
			// L <- 116*Y^(1/3)-16   for Y>0.008856
			// L <- 903.3*Y      for Y<=0.008856
			//
			// u' <- 4*X/(X + 15*Y + 3*Z)
			// v' <- 9*Y/(X + 15*Y + 3*Z)
			//
			// u <- 13*L*(u' - un), where un=0.19793943
			// v <- 13*L*(v' - vn), where vn=0.46831096
			//
			// On output 0 <= L <= 100, -134 <= u <= 220, -140 <= v <= 122
			// The values are then converted to the destination data type:
			// 8-bit images:
			//   L <- L*255/100, u <- (u + 134)*255/354, v <- (v + 140)*255/256
			// 16-bit images are currently not supported
			// 32-bit images:
			//   L, u, v are left as is

			tmp_color tmp = 
				RgbToXyz<tmp_color>()( DefaultConverter<tmp_color, T>()(from) );

			const tmp_type &X = tmp[0];
			const tmp_type &Y = tmp[1];
			const tmp_type &Z = tmp[2];

			tmp_type L = (Y > 0.008856) ? 
				116 * std::pow(Y, static_cast<tmp_type>(1./3)) - 16 : 
				903.3 * Y;
			tmp_type u_ = 4 * X / (X + 15*Y + 3*Z);
			tmp_type v_ = 9 * Y / (X + 15*Y + 3*Z);

			const tmp_type un = 0.19793943;
			const tmp_type vn = 0.46831096;

			tmp_type u = 13 * L * (u_-un);
			tmp_type v = 13 * L * (v_-vn);
			
			return To(
				static_cast<value_type>(L),
				static_cast<value_type>(u),
				static_cast<value_type>(v)
			);
		}
	};
	template<typename T> struct RgbToLuv< Color<T, 2> >;
	
	template<typename T>
	struct LuvToRgb {
		typedef T To;
		typedef T From;

		typedef typename ColorTrait<T>::BaseType value_type;
		typedef typename TypeTrait<value_type>::ExtendedType tmp_type;
		typedef typename ColorTrait<T>::ExtendedColor tmp_color;;

		const To operator()(const From &from) const 
		{
			// TODO
			assert(0);
		}
	};
	template<typename T> struct LuvToRgb< Color<T, 2> >;
}

#endif
