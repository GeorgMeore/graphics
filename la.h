typedef struct {
	F64 x, y, z;
} Vec;

Vec vsub(Vec a, Vec b);
Vec vadd(Vec a, Vec b);
Vec vmul(Vec a, F64 c);
Vec cross(Vec a, Vec b);
F64 dot(Vec a, Vec b);

Vec reflect(Vec v, Vec n);
Vec refract(Vec v, Vec n, F64 eta);

typedef struct {
	F64 m[3][3];
} Mat;

Mat mmul(Mat a, Mat b);
Mat transp(Mat m);
Vec mapply(Mat m, Vec v);
F64 det(Mat m);

Mat rotx(F64 a);
Mat roty(F64 a);
Mat rotz(F64 a);
