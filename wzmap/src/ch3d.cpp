/* PROGRAM: ch3d.c
* PURPOSE: Module for 3D convex hull algorithm
* AUTHOR:  Geoffrey Card
* DATE:    2014-10-13 -
* NOTES:   Time: O(n^2*log(n))    Algorithm requires sort(n) for each n, hence n*n*log(n).
*          Space: O(n)
* TO DO:
*/

#include "ch3d.h"
#include "q3map2.h"

using namespace std;

#if 0
void CrossProduct(float* a, float* b, float* c) {
	c[0] = a[1] * b[2] - a[2] * b[1];
	c[1] = a[2] * b[0] - a[0] * b[2];
	c[2] = a[0] * b[1] - a[1] * b[0];
}

float DotProduct(float* a, float* b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
#endif

void UnitVector(float* u) {
	float temp;

	temp = sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
	u[0] /= temp;
	u[1] /= temp;
	u[2] /= temp;
}

float TripleProduct(float* a, float* b, float* c) {
	return a[0] * (b[1] * c[2] - b[2] * c[1]) + a[1] * (b[2] * c[0] - b[0] * c[2]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
}

void GeneratePoints(float* p, unsigned int size, int convexset) {
	float a1 = 0;
	float a2 = 0;
	unsigned int i = 0;

	if (convexset == TRUE) {
		// convex set
		for (i = 0; i < size; i++) {
			a1 = 2 * PI*((float)rand() / RAND_MAX);
			a2 = asin(2 * ((float)rand() / RAND_MAX) - 1);
			p[i * 3 + 0] = sin(a2)*cos(a1);
			p[i * 3 + 1] = sin(a2)*sin(a1);
			p[i * 3 + 2] = cos(a2);
		}
	}
	else {
		// random points
		for (i = 0; i < size; i++) {
			p[i * 3 + 0] = ((float)rand() / RAND_MAX);
			p[i * 3 + 1] = ((float)rand() / RAND_MAX);
			p[i * 3 + 2] = ((float)rand() / RAND_MAX);
		}
	}
}

void PrintPoints(float* p, unsigned int lenp) {
	unsigned int i = 0;

	printf("POINTS\n");
	printf("--------+---------------------------\n");
	for (i = 0; i < lenp; i++) {
		printf("%5u   |   %6.2f   %6.2f   %6.2f\n", i, p[i * 3 + 0], p[i * 3 + 1], p[i * 3 + 2]);
	}
	printf("\n");
}

void PrintList(float* p, unsigned int lenp, unsigned int* l, unsigned int lenl, unsigned int leni) {
	unsigned int i = 0;
	unsigned int j = 0;

	if (leni == 1) {
		printf("VERTICES\n");
	}
	else if (leni == 2) {
		printf("EDGES\n");
	}
	else if (leni == 3) {
		printf("FACETS\n");
	}
	else {
		printf("List contains %u unknown items of length %u.\n\n", lenl, leni);
		return;
	}

	// seperator
	printf("--------+");
	for (j = 0; j < leni; j++) {
		printf("--------");
	}
	printf("---+");
	for (j = 0; j < leni; j++) {
		printf("---------");
		printf("---------");
		printf("---------");
		printf("---+");
	}
	printf("\n");

	// list
	for (i = 0; i < lenl; i++) {
		// index
		printf("%5u   |", i);


		// indices
		for (j = 0; j < leni; j++) {
			printf("   %5u", l[i*leni + j]);
		}
		printf("   |");

		// points
		for (j = 0; j < leni; j++) {
			if (l[i*leni + j] < lenp) {
				printf("   %6.2f   %6.2f   %6.2f   |", p[l[i*leni + j] * 3 + 0], p[l[i*leni + j] * 3 + 1], p[l[i*leni + j] * 3 + 2]);
			}
			else {
				break;
			}
		}
		printf("\n");
	}
	printf("\n");
}

void PrintResults(float* p, unsigned int lenp, unsigned int* l, unsigned int lenl, unsigned int leni) {
	PrintPoints(p, lenp);
	PrintList(p, lenp, l, lenl, leni);
}

void RemoveRepetitions(float** p_p, unsigned int* lenp_p) {
	float* p = NULL;
	unsigned int lenp = 0;
	unsigned int* s1 = NULL;
	unsigned int* s2 = NULL;
	unsigned int* temp = NULL;
	unsigned int a = 0;
	unsigned int i = 0;
	unsigned int i1 = 0;
	unsigned int i2 = 0;
	unsigned int lim1 = 0;
	unsigned int lim2 = 0;

#ifdef DEBUG
	printf("DEBUG: RemoveRepetitions\n");
#endif /* DEBUG */

	// get points
	p = *p_p;
	lenp = *lenp_p;

	// initialize indices
	s1 = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	memset(s1, 0, lenp * sizeof(unsigned int));
	for (i = 0; i < lenp; i++) {
		s1[i] = i;
	}
	// s2 can be garbage, will be replaced
	s2 = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	memset(s2, 0, lenp * sizeof(unsigned int));

	// sort
	a = 1;
	while (a < lenp * 2) {
		i = 0;
		while (i < lenp) {
			lim1 = i + 1 * a < lenp ? i + 1 * a : lenp;
			lim2 = i + 2 * a < lenp ? i + 2 * a : lenp;
			i1 = i;
			i2 = lim1;
			while (i < lim2) {
				if (i1 == lim1) {
					s2[i] = s1[i2];
					i2++;
				}
				else if (i2 == lim2) {
					s2[i] = s1[i1];
					i1++;
				}
				else if (p[s1[i1] * 3 + 0] < p[s1[i2] * 3 + 0]) {
					s2[i] = s1[i1];
					i1++;
				}
				else if (p[s1[i1] * 3 + 0] > p[s1[i2] * 3 + 0]) {
					s2[i] = s1[i2];
					i2++;
				}
				else if (p[s1[i1] * 3 + 1] < p[s1[i2] * 3 + 1]) {
					s2[i] = s1[i1];
					i1++;
				}
				else if (p[s1[i1] * 3 + 1] > p[s1[i2] * 3 + 1]) {
					s2[i] = s1[i2];
					i2++;
				}
				else if (p[s1[i1] * 3 + 2] < p[s1[i2] * 3 + 2]) {
					i1++;
				}
				else if (p[s1[i1] * 3 + 2] > p[s1[i2] * 3 + 2]) {
					s2[i] = s1[i2];
					s2[i] = s1[i1];
					i2++;
				}
				else {
					s2[i] = s1[i1];
					i1++;
				}
				i++;
			}
		}
		a *= 2;
		// exchange addresses
		temp = s1;
		s1 = s2;
		s2 = temp;
	}

	// repurpose s2 to mark repeats
	// TRUE == repeat
	for (i = 0; i < lenp - 1; i++) {
		if (p[s1[i] * 3 + 0] == p[s1[i + 1] * 3 + 0] && p[s1[i] * 3 + 1] == p[s1[i + 1] * 3 + 1] && p[s1[i] * 3 + 2] == p[s1[i + 1] * 3 + 2]) {
			s2[s1[i]] = TRUE;
		}
		else {
			s2[s1[i]] = FALSE;
		}
	}
	s2[s1[lenp - 1]] = FALSE; // this point isn't covered in the loop

							  // repurpose a as last valid index
	a = lenp - 1;
	for (i = lenp - 1; i < lenp; i--) {
		if (s2[i] == TRUE) {
			// place point out of range
			// really, just copy out-of-range point into it's place
			p[i * 3 + 0] = p[a * 3 + 0];
			p[i * 3 + 1] = p[a * 3 + 1];
			p[i * 3 + 2] = p[a * 3 + 2];
			a--;
		}
	}

	free(s1);
	free(s2);
	s1 = NULL;
	s2 = NULL;
	temp = NULL;

	lenp = a + 1;
	p = (float*)realloc(p, 3 * lenp * sizeof(float));
	*p_p = p;
	*lenp_p = lenp;
}

void SortForIndices(float* l1, unsigned int* s1, unsigned int* s2, unsigned int size) {
	unsigned int* temps = NULL; // just a pointer, not an array
	unsigned int a = 0;
	unsigned int i = 0;
	unsigned int i1 = 0;
	unsigned int i2 = 0;
	unsigned int lim1 = 0;
	unsigned int lim2 = 0;

	a = 1;
	while (a < size * 2) {
		i = 0;
		while (i < size) {
			lim1 = i + 1 * a < size ? i + 1 * a : size;
			lim2 = i + 2 * a < size ? i + 2 * a : size;
			i1 = i;
			i2 = lim1;
			while (i < lim2) {
				if (i1 == lim1) {
					s2[i] = s1[i2];
					i2++;
				}
				else if (i2 == lim2) {
					s2[i] = s1[i1];
					i1++;
				}
				else if (l1[s1[i1]] <= l1[s1[i2]]) {
					s2[i] = s1[i1];
					i1++;
				}
				else {
					s2[i] = s1[i2];
					i2++;
				}
				i++;
			}
		}
		a *= 2;
		temps = s1;
		s1 = s2;
		s2 = temps;
	}
}

unsigned int LinearCheck(float* p, unsigned int lenp) {
	float u0[3] = { 0 };
	float u1[3] = { 0 };
	unsigned int i = 0;
	float tempf = 0;

#ifdef DEBUG
	printf("DEBUG: LinearCheck\n");
#endif /* DEBUG */

	u0[0] = p[1 * 3 + 0] - p[0 * 3 + 0];
	u0[1] = p[1 * 3 + 1] - p[0 * 3 + 1];
	u0[2] = p[1 * 3 + 2] - p[0 * 3 + 2];
	UnitVector(u0);
	for (i = 2; i < lenp; i++) {
		u1[0] = p[i * 3 + 0] - p[0 * 3 + 0];
		u1[1] = p[i * 3 + 1] - p[0 * 3 + 1];
		u1[2] = p[i * 3 + 2] - p[0 * 3 + 2];
		UnitVector(u1);
		tempf = DotProduct(u0, u1);
		if (tempf < 0) tempf *= -1;
#ifdef DEBUG
		printf("DEBUG: %7.3f %7.3f %7.3f\n", u0[0], u0[1], u0[2]);
		printf("DEBUG: %7.3f %7.3f %7.3f\n", u1[0], u1[1], u1[2]);
		printf("DEBUG: %30.20f\n", tempf);
		printf("DEBUG: %30.20f\n", 1 - CH3D_EPSILON);
#endif /* DEBUG */
		// if the cross product is NOT ZERO
		// then not linear
		if (tempf <= 1 - CH3D_EPSILON) {
#ifdef DEBUG
			printf("DEBUG: Non-linear\n");
#endif /* DEBUG */
			return FALSE;
		}
	}
#ifdef DEBUG
	printf("DEBUG: Linear\n");
#endif /* DEBUG */
	return TRUE;
}

unsigned int PlanarCheck(float* p, unsigned int lenp) {
	float u0[3] = { 0 };
	float u1[3] = { 0 };
	float u2[3] = { 0 };
	unsigned int i = 0;
	float tempf = 0;

#ifdef DEBUG
	printf("DEBUG: PlanarCheck\n");
#endif /* DEBUG */

	u0[0] = p[1 * 3 + 0] - p[0 * 3 + 0];
	u0[1] = p[1 * 3 + 1] - p[0 * 3 + 1];
	u0[2] = p[1 * 3 + 2] - p[0 * 3 + 2];
	u1[0] = p[2 * 3 + 0] - p[0 * 3 + 0];
	u1[1] = p[2 * 3 + 1] - p[0 * 3 + 1];
	u1[2] = p[2 * 3 + 2] - p[0 * 3 + 2];
	CrossProduct(u0, u1, u2);
	UnitVector(u2);
	for (i = 3; i < lenp; i++) {
		// repurpose u0
		u0[0] = p[i * 3 + 0] - p[0 * 3 + 0];
		u0[1] = p[i * 3 + 1] - p[0 * 3 + 1];
		u0[2] = p[i * 3 + 2] - p[0 * 3 + 2];
		UnitVector(u0);
		tempf = DotProduct(u0, u2);
		if (tempf < 0) tempf *= -1;
#ifdef DEBUG
		printf("DEBUG: u2 = %6.2f %6.2f %6.2f\n", u2[0], u2[1], u2[2]);
		printf("DEBUG: u0 = %6.2f %6.2f %6.2f\n", u0[0], u0[1], u0[2]);
		printf("DEBUG: %30.20f\n", tempf);
		printf("DEBUG: %30.20f\n", CH3D_EPSILON);
#endif /* DEBUG */
		// if u0.u2 > 0, then not planar
		if (tempf > CH3D_EPSILON) {
#ifdef DEBUG
			printf("DEBUG: Non-planar\n");
#endif /* DEBUG */
			return FALSE;
		}
	}
#ifdef DEBUG
	printf("DEBUG: Planar\n");
#endif /* DEBUG */
	return TRUE;
}

unsigned int ConvexityTest2D(float* v, float* v0, float* v1, float* v2) {
	float v3[3] = { 0 };
	float v4[3] = { 0 };

	// planar
	v3[0] = v2[0] - v1[0];
	v3[1] = v2[1] - v1[1];
	v3[2] = v2[2] - v1[2];
	v4[0] = v0[0] - v1[0];
	v4[1] = v0[1] - v1[1];
	v4[2] = v0[2] - v1[2];

	if (TripleProduct(v3, v4, v) > CH3D_EPSILON) {
		return CONVEX;
	}
	else {
		return CONCAVE;
	}
}

unsigned int ConvexityTest3D(float* v, float* v0, float* v1, float* v2) {
	float v3[3] = { 0 };
	float v4[3] = { 0 };
	float test = 0;

	// check whether v_1 is above (+), in (0), or below (-) the plane of v_0 and v_2
	// test = v1 . (v0 x v2)
	test = TripleProduct(v1, v0, v2);

	if (test > CH3D_EPSILON) {
		// above the plane
		return CONVEX;
	}
	else if (-CH3D_EPSILON < test && test <= CH3D_EPSILON) {
		// planar
		// is it farther out than v_0 and v_2 (convex in the plane)?
		// v4 = v2 - v0
		// v5 = v4 x v
		v3[0] = v2[0] - v0[0];
		v3[1] = v2[1] - v0[1];
		v3[2] = v2[2] - v0[2];
		CrossProduct(v3, v, v4);
		// if v5 . v1 > v5 . v0 (or v5 . v2) then it is convex planar
		if (DotProduct(v4, v1) > DotProduct(v4, v0)) {
			return PLANAR;
		}
		else {
			return CONCAVE;
		}
	}
	else {
		return CONCAVE;
	}
}

void BuildPoint(float* p, unsigned int lenp, unsigned int** l_p, unsigned int* lenl_p, unsigned int* leni_p, int listtype) {
	unsigned int* l = NULL;
	unsigned int lenl = 0;
	unsigned int leni = 0;

#ifdef DEBUG
	printf("DEBUG: Point\n");
#endif /* DEBUG */

	lenl = 1;
	leni = 1;
	l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
	l[0] = 0;
	*leni_p = leni;
	*lenl_p = lenl;
	*l_p = l;
}

void BuildLine(float* p, unsigned int lenp, unsigned int** l_p, unsigned int* lenl_p, unsigned int* leni_p, int listtype) {
	unsigned int* l = NULL;
	unsigned int lenl = 0;
	unsigned int leni = 0;

#ifdef DEBUG
	printf("DEBUG: Line\n");
#endif /* DEBUG */

	if (listtype == VERTICES) {
		lenl = 2;
		leni = 1;
	}
	else {
		lenl = 1;
		leni = 2;
	}
	l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
	l[0] = 0;
	l[1] = 1;
	*leni_p = leni;
	*lenl_p = lenl;
	*l_p = l;
}

void BuildLinear(float* p, unsigned int lenp, unsigned int** l_p, unsigned int* lenl_p, unsigned int* leni_p, int listtype) {
	unsigned int* l = NULL;
	unsigned int lenl = 0;
	unsigned int leni = 0;
	float u0[3] = { 0 };
	float u1[3] = { 0 };
	unsigned int i = 0;
	unsigned int i0 = 0;
	unsigned int i1 = 0;
	float val0 = 0;
	float val1 = 0;
	float tempf = 0;

#ifdef DEBUG
	printf("DEBUG: Linear\n");
#endif /* DEBUG */

	if (listtype == VERTICES) {
		lenl = 2;
		leni = 1;
	}
	else {
		lenl = 1;
		leni = 2;
	}
	l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));

	u0[0] = p[1 * 3 + 0] - p[0 * 3 + 0];
	u0[1] = p[1 * 3 + 1] - p[0 * 3 + 1];
	u0[2] = p[1 * 3 + 2] - p[0 * 3 + 2];

	// find extremes
	// repurposing q1, q2 as indices for extremes
	// repurposing c[0], c[1] as values of extremes
	i0 = 0;
	val0 = 0;
	i1 = 1;
	val1 = DotProduct(u0, u0);
	for (i = 2; i < lenp; i++) {
		u1[0] = p[i * 3 + 0] - p[0 * 3 + 0];
		u1[1] = p[i * 3 + 1] - p[0 * 3 + 1];
		u1[2] = p[i * 3 + 2] - p[0 * 3 + 2];
		// get relative position
		tempf = DotProduct(u0, u1);
		if (tempf < val0) {
			// if farthest negative
			i0 = i;
			val0 = tempf;
		}
		else if (tempf > val1) {
			// if farthest forward
			i1 = i;
			val1 = tempf;
		}
	}

	l[0] = i0;
	l[1] = i1;
	*leni_p = leni;
	*lenl_p = lenl;
	*l_p = l;
}

void BuildTriangle(float* p, unsigned int lenp, unsigned int** l_p, unsigned int* lenl_p, unsigned int* leni_p, int listtype) {
	unsigned int* l = NULL;
	unsigned int lenl = 0;
	unsigned int leni = 0;

#ifdef DEBUG
	printf("DEBUG: Triangle\n");
#endif /* DEBUG */

	if (listtype == VERTICES) {
		lenl = 3;
		leni = 1;
		l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
		l[0] = 0;
		l[1] = 1;
		l[2] = 2;
	}
	else if (listtype == EDGES) {
		lenl = 3;
		leni = 2;
		l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
		l[0] = 0;
		l[1] = 1;
		l[2] = 0;
		l[3] = 2;
		l[4] = 1;
		l[5] = 2;
	}
	else {
		lenl = 1; // from a pure maths perspective, this should be two (one for each side), but frak it
		leni = 3;
		l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
		l[0] = 0;
		l[1] = 1;
		l[2] = 2;
	}

	*leni_p = leni;
	*lenl_p = lenl;
	*l_p = l;
}

void BuildPlanar(float* p, unsigned int lenp, unsigned int** l_p, unsigned int* lenl_p, unsigned int* leni_p, int listtype) {
	unsigned int* l = NULL;
	unsigned int lenl = 0;
	unsigned int leni = 0;
	float c[3] = { 0 };
	float u0[3] = { 0 };
	float u1[3] = { 0 };
	float u2[3] = { 0 };
	float* v = NULL;
	float* theta = NULL;
	unsigned int* s = NULL;
	unsigned int* g = NULL;
	unsigned int* t = NULL;
	unsigned int i = 0;
	unsigned int i0 = 0;
	unsigned int i1 = 0;
	unsigned int i2 = 0;
	unsigned int i3 = 0;
	unsigned int imax = 0;
	unsigned int tcount = 0;
	unsigned int chk1 = 0;
	unsigned int chk2 = 0;

#ifdef DEBUG
	printf("DEBUG: Planar\n");
#endif /* DEBUG */

	// get direction vectors
	u0[0] = p[1 * 3 + 0] - p[0 * 3 + 0];
	u0[1] = p[1 * 3 + 1] - p[0 * 3 + 1];
	u0[2] = p[1 * 3 + 2] - p[0 * 3 + 2];
	u1[0] = p[2 * 3 + 0] - p[0 * 3 + 0];
	u1[1] = p[2 * 3 + 1] - p[0 * 3 + 1];
	u1[2] = p[2 * 3 + 2] - p[0 * 3 + 2];
	UnitVector(u0); // might be unnecessary
	UnitVector(u1); // might be unnecessary
	CrossProduct(u0, u1, u2);
	CrossProduct(u2, u0, u1);
	// u2 is normal to plane

	// calculate approximate centroid
	c[0] = p[0 * 3 + 0];
	c[1] = p[0 * 3 + 1];
	c[2] = p[0 * 3 + 2];
	for (i = 1; i < lenp; i++) {
		c[0] += p[i * 3 + 0];
		c[1] += p[i * 3 + 1];
		c[2] += p[i * 3 + 2];
	}
	c[0] /= lenp;
	c[1] /= lenp;
	c[2] /= lenp;

	// allocate lists
	v = (float*)malloc(3 * lenp * sizeof(float));
	theta = (float*)malloc(lenp * sizeof(float));
	s = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	g = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	t = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	// iniitalize lists
	for (i = 0; i < lenp; i++) {
		v[i * 3 + 0] = p[i * 3 + 0] - c[0];
		v[i * 3 + 1] = p[i * 3 + 1] - c[1];
		v[i * 3 + 2] = p[i * 3 + 2] - c[2];
		theta[i] = atan2(DotProduct(u1, &v[i * 3]),
			DotProduct(u0, &v[i * 3]));
		s[i] = i;
		t[i] = CONVEX;
	}
	// create list of indices of sorted theta
	SortForIndices(theta, s, g, lenp);

#ifdef DEBUG
	printf("     | ");
	for (i = 0; i < lenp; i++) {
		printf("%4d", i);
	}
	printf("    | ");
	for (i = 0; i < lenp; i++) {
		printf("%4d", s[i]);
	}
	printf("\n");

	// separator
	printf("-----+-");
	for (i = 0; i < lenp; i++) {
		printf("----");
	}
	printf("----+");
	for (i = 0; i < lenp; i++) {
		printf("----");
	}
	printf("-\n");
#endif /* DEBUG */

	tcount = 0;

	// create a set of sequential indices
	// only count values that haven't failed the test
	// use +=lenp-1 instead of -=1
	//  i_???? = i0
	// 	i_prev = i1
	// 	i_curr = i2
	// 	i_next = i3
	i0 = 0;
	while (t[s[i0%lenp]] == CONCAVE) i0++;
	i1 = i0 + 1;
	while (t[s[i1%lenp]] == CONCAVE) i1++;
	i2 = i1 + 1;;
	while (t[s[i2%lenp]] == CONCAVE) i2++;
	i3 = i2 + 1;
	while (t[s[i3%lenp]] == CONCAVE) i3++;
	imax = i2 + lenp;
#ifdef DEBUG
	printf("%4u | ", s[i2%lenp]);
	// test results, of this round
	for (i = 0; i < lenp; i++) {
		printf("%4u", t[i]);
	}
	printf("    | ");
	for (i = 0; i < lenp; i++) {
		printf("%4u", t[s[i]]);
	}
	printf("\n");
#endif /* DEBUG */
	// go through the whole list once
	// check forward and backward at each failure
	// (until success in both directions)
	while (i2 < imax) {

		// test v1
		t[s[i2%lenp]] = ConvexityTest2D(u2, &v[s[i1%lenp] * 3], &v[s[i2%lenp] * 3], &v[s[i3%lenp] * 3]);

#ifdef DEBUG
		printf("%4u | ", s[i2%lenp]);
		// test results, of this round
		for (i = 0; i < lenp; i++) {
			printf("%4u", t[i]);
		}
		printf("    | ");
		for (i = 0; i < lenp; i++) {
			printf("%4u", t[s[i]]);
		}
		printf(" L\n");
#endif /* DEBUG */

		if (t[s[i2%lenp]] == CONCAVE) {
			// concavity detected

			// failsafe
			tcount++;
			if (tcount >= lenp) {
				break;
			}

			// increment forward
			i2 = i3;
			i3++;
			while (t[s[i3%lenp]] == CONCAVE) i3++;

			// check forward and backward until coast is clear
			chk1 = CONCAVE;
			chk2 = CONCAVE;
			while (chk1 == CONCAVE || chk2 == CONCAVE) {
				// check backward
				chk1 = ConvexityTest2D(u2, &v[s[i0%lenp] * 3], &v[s[i1%lenp] * 3], &v[s[i2%lenp] * 3]);
				if (chk1 == CONCAVE) {
					t[s[i1%lenp]] = CONCAVE;
					// failsafe
					tcount++;
					if (tcount >= lenp) {
						break;
						break;
					}
					i1 = i0;
					i0 += lenp - 1; // equvalent to -=1, but works for unsigned
					while (t[s[i0%lenp]] == CONCAVE) i0 += lenp - 1;
				}
				// check forward
				chk2 = ConvexityTest2D(u2, &v[s[i1%lenp] * 3], &v[s[i2%lenp] * 3], &v[s[i3%lenp] * 3]);
				if (chk2 == CONCAVE) {
					t[s[i2%lenp]] = CONCAVE;
					// failsafe
					tcount++;
					if (tcount >= lenp) {
						break;
						break;
					}
					i2 = i3;
					i3++;
					while (t[s[i3%lenp]] == CONCAVE) i3++;
				}
#ifdef DEBUG
				printf("%4u | ", s[i2%lenp]);
				// test results, of this round
				for (i = 0; i < lenp; i++) {
					printf("%4u", t[i]);
				}
				printf("    | ");
				for (i = 0; i < lenp; i++) {
					printf("%4u", t[s[i]]);
				}
				printf(" F\n");
#endif /* DEBUG */
			}
			// concavity removed
		}
		else {
			// no concavity, continue
			i0 = i1;
			i1 = i2;
			i2 = i3;
			i3++;
			while (t[s[i3%lenp]] == CONCAVE) i3++;
		}
	}

#ifdef DEBUG
	printf("%4u | ", s[i2%lenp]);
	// test results, of this round
	for (i = 0; i < lenp; i++) {
		printf("%4u", t[i]);
	}
	printf("    | ");
	for (i = 0; i < lenp; i++) {
		printf("%4u", t[s[i]]);
	}
	printf("\n");
#endif /* DEBUG */

	// failsafe
	if (tcount >= lenp) {
#ifdef DEBUG
		printf("DEBUG: FAILSAFE ENGAGED\n");
#endif /* DEBUG */
	}
	else {
		// create vertices, edges, facets
		if (listtype == VERTICES) {
			// VERTICES
			// list this vertex, push the rest to the queue (they'll get added on their turn
			leni = 1;
			lenl = lenp - tcount;
			l = (unsigned int*)malloc(lenl*leni * sizeof(unsigned int));
			i0 = 0;
			for (i = 0; i < lenp; i++) {
				if (t[s[i]] != CONCAVE) {
					l[i0++] = s[i];
				}
			}
		}
		else if (listtype == EDGES) {
			// EDGES
			// list edges
			leni = 2;
			lenl = lenp - tcount;
			l = (unsigned int*)malloc(lenl*leni * sizeof(unsigned int));
			i1 = 0;
			while (t[s[i1%lenp]] == CONCAVE) i1++;
			i2 = i1 + 1;
			while (t[s[i2%lenp]] == CONCAVE) i2++;
			// count number of lines created
			i = 0;
			while (i < lenl) {
				l[i*leni + 0] = s[i1%lenp];
				l[i*leni + 1] = s[i2%lenp];
				i++;
				i1 = i2;
				i2++;
				while (t[s[i2%lenp]] == CONCAVE) i2++;
			}
		}
		else {
			// FACETS
			// create a triangle fan of points that pass
			leni = 3;
			lenl = lenp - tcount - 2;
			l = (unsigned int*)malloc(lenl*leni * sizeof(unsigned int));
			i0 = 0;
			while (t[s[i0%lenp]] == CONCAVE) i0++;
			i1 = i0 + 1;
			while (t[s[i1%lenp]] == CONCAVE) i1++;
			i2 = i1 + 1;
			while (t[s[i2%lenp]] == CONCAVE) i2++;
			// count number of lines created
			i = 0;
			while (i < lenl) {
				l[i*leni + 0] = s[i0%lenp];
				l[i*leni + 1] = s[i1%lenp];
				l[i*leni + 2] = s[i2%lenp];
				i++;
				i1 = i2;
				i2++;
				while (t[s[i2%lenp]] == CONCAVE) i2++;
			}
		}
	}

	free(v);
	free(theta);
	free(s);
	free(g);
	free(t);

	*leni_p = leni;
	*lenl_p = lenl;
	*l_p = l;
}

void BuildTetrahedron(float* p, unsigned int lenp, unsigned int** l_p, unsigned int* lenl_p, unsigned int* leni_p, int listtype) {
	unsigned int* l = NULL;
	unsigned int lenl = 0;
	unsigned int leni = 0;

#ifdef DEBUG
	printf("DEBUG: Tetrahedron\n");
#endif /* DEBUG */

	if (listtype == VERTICES) {
		lenl = 4;
		leni = 1;
		l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
		l[0] = 0;
		l[1] = 1;
		l[2] = 2;
		l[3] = 3;
	}
	else if (listtype == EDGES) {
		lenl = 6;
		leni = 2;
		l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
		l[0] = 0;
		l[1] = 1;
		l[2] = 0;
		l[3] = 2;
		l[4] = 0;
		l[5] = 3;
		l[6] = 1;
		l[7] = 2;
		l[8] = 1;
		l[9] = 3;
		l[10] = 2;
		l[11] = 3;
	}
	else {
		lenl = 4;
		leni = 3;
		l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
		l[0] = 0;
		l[1] = 1;
		l[2] = 2;
		l[3] = 0;
		l[4] = 1;
		l[5] = 3;
		l[6] = 0;
		l[7] = 2;
		l[8] = 3;
		l[9] = 1;
		l[10] = 2;
		l[11] = 3;
	}

	*leni_p = leni;
	*lenl_p = lenl;
	*l_p = l;
}

void BuildHull(float* p, unsigned int lenp, unsigned int** l_p, unsigned int* lenl_p, unsigned int* leni_p, int listtype) {
	// arrays and lengths
	unsigned int* l = NULL;
	unsigned int leni = 0;
	unsigned int lenl = 0;
	unsigned int origLenl = *lenl_p * *leni_p;

	// queue
	int* visited = NULL;
	unsigned int* q = NULL;
	unsigned int q1 = 0;
	unsigned int q2 = 0;

	// points and vectors
	float c[3] = { 0 };
	float vii[3] = { 0 };
	float u0[3] = { 0 };
	float u1[3] = { 0 };
	float* v = NULL;

	// lists
	float* theta = NULL;
	unsigned int* s = NULL;
	unsigned int* s2 = NULL;
	unsigned int* t = NULL;
	unsigned int tcount = 0;
	float* n = NULL;
	unsigned int lenn1 = 0;
	unsigned int lenn2 = 0;

	// indices
	unsigned int i = 0;
	unsigned int ii = 0;
	unsigned int i0 = 0;
	unsigned int i1 = 0;
	unsigned int i2 = 0;
	unsigned int i3 = 0;
	unsigned int imax = 0;

	// ????
	float tempf = 0;

	// bools for checks
	unsigned int chk1;
	unsigned int chk2;

#ifdef DEBUG
	printf("DEBUG: Hull\n");
#endif /* DEBUG */

	// Determine maximum possible number of edges (E) and facets (F)
	// E <= V + (V-1) + (V-2) + ... + 0 = V*(V-1)/2
	// F = 2 + E - V
	// F <= 2 + V*(V-1)/2 - V
	// F <= 2+V*(V-3)/2
	// Then I did some maths and discovered that if you're building polyhedra from a minimal number of triangles:
	// E = 3*V-6
	// F = 2*V-4
	if (listtype == VERTICES) {
		leni = 1;
		lenl = lenp;
		l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
	}
	else if (listtype == EDGES) {
		leni = 2;
		lenl = leni*(3 * lenp - 6);
		l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
	}
	else {
		leni = 3;
		lenl = leni*(2 * lenp - 4);
		//l = (unsigned int*)malloc(leni*lenl * sizeof(unsigned int));
		l = (unsigned int*)malloc(64*leni*lenl * sizeof(unsigned int)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...
	}
	lenl = 0; // becomes incrementor for l

			  // allocate everything
	//visited = (int*)malloc(lenp * sizeof(int));
	visited = (int*)malloc(3 * lenp * sizeof(int)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...
	//q = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	q = (unsigned int*)malloc(3 * lenp * sizeof(unsigned int)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...
	//v = (float*)malloc(3 * lenp * sizeof(float));
	v = (float*)malloc(9 * lenp * sizeof(float)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...
	//theta = (float*)malloc(lenp * sizeof(float));
	theta = (float*)malloc(3 * lenp * sizeof(float)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...
	//s = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	s = (unsigned int*)malloc(3 * lenp * sizeof(unsigned int)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...
	//s2 = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	s2 = (unsigned int*)malloc(3 * lenp * sizeof(unsigned int)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...
	//t = (unsigned int*)malloc(lenp * sizeof(unsigned int));
	t = (unsigned int*)malloc(3 * lenp * sizeof(unsigned int)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...
	//n = (float*)malloc(3 * (2 * lenp - 4) * sizeof(float));
	n = (float*)malloc(64 * (2 * lenp - 4) * sizeof(float)); // UQ1: because this library is buggy... Leave space to overrun original array and use exceptions...

	try {
		try {

			// calculate approximate centroid and get first point
			c[0] = p[0];
			c[1] = p[1];
			c[2] = p[2];
			// find index of the lowerest point p_x_min
			ii = 0;
			for (i = 1; i < lenp; i++) {
				// visited
				visited[i] = UNVISITED;
				// center
				c[0] += p[i * 3 + 0];
				c[1] += p[i * 3 + 1];
				c[2] += p[i * 3 + 2];
				// find min x
				if (p[i * 3 + 0] < p[ii * 3 + 0]) ii = i;
			}
			c[0] /= lenp;
			c[1] /= lenp;
			c[2] /= lenp;


			// push first point
			visited[ii] = QUEUED;
			q1 = 0;
			q2 = 0;
			q[q2++] = ii;

			// set list of normals, n, as empty
			lenn1 = 0;
			lenn2 = 0;

			// while the queue is not empty
			while (q1 < q2) {
				// pull point
				ii = q[q1++];

#ifdef DEBUG
				printf("DEBUG: POINT LOOP %u\n", ii);
#endif /* DEBUG */

				// get directional vectors
				vii[0] = p[ii * 3 + 0] - c[0];
				vii[1] = p[ii * 3 + 1] - c[1];
				vii[2] = p[ii * 3 + 2] - c[2];
				if (-CH3D_EPSILON < vii[0] && vii[0] <= CH3D_EPSILON) {
					u0[0] = 1;
					u0[1] = 0;
					u0[2] = 0;
				}
				else if (-CH3D_EPSILON < vii[1] && vii[1] <= CH3D_EPSILON) {
					u0[0] = 0;
					u0[1] = 1;
					u0[2] = 0;
				}
				else {
					u0[0] = vii[1];
					u0[1] = -vii[0];
					u0[2] = 0;
					// this verison of UnitVector is slightly more optimized
					// don't both using the function here
					tempf = sqrt(u0[0] * u0[0] + u0[1] * u0[1]);
					u0[0] /= tempf;
					u0[1] /= tempf;
				}
				// u1 = v x u0
				CrossProduct(vii, u0, u1);
				UnitVector(u1);

#ifdef DEBUG
				printf("vii = %6.3f %6.3f %6.3f\n", vii[0], vii[1], vii[2]);
				printf("u0  = %6.3f %6.3f %6.3f\n", u0[0], u0[1], u0[2]);
				printf("u1  = %6.3f %6.3f %6.3f\n", u1[0], u1[1], u1[2]);
				printf("\n");
#endif /* DEBUG */

				// create a list of vectors outward from v
				// calculate their angle around v
				// create a list of test results
				for (i = 0; i < lenp; i++) {
					v[i * 3 + 0] = p[i * 3 + 0] - p[ii * 3 + 0];
					v[i * 3 + 1] = p[i * 3 + 1] - p[ii * 3 + 1];
					v[i * 3 + 2] = p[i * 3 + 2] - p[ii * 3 + 2];
					theta[i] = atan2(
						u1[0] * v[i * 3 + 0] + u1[1] * v[i * 3 + 1] + u1[2] * v[i * 3 + 2],
						u0[0] * v[i * 3 + 0] + u0[1] * v[i * 3 + 1] + u0[2] * v[i * 3 + 2]);
					t[i] = CONVEX;
					s[i] = i;
				}
				// create list of indices of sorted theta
				SortForIndices(theta, s, s2, lenp);
				// and mark vii as failed
				t[ii] = CONCAVE;

#ifdef DEBUG
				for (i = 0; i < lenp; i++) {
					printf("%4u %6.3f | %4u %6.3f\n", i, theta[i], s[i], theta[s[i]]);
				}
				printf("\n");
#endif /* DEBUG */

				// tcount is a failsafe
				// it counts the number of failures
				// if tcount >= lenp, there will be an infinite loop
				// therefore break, and skip to the end
				tcount = 1;

#ifdef DEBUG
				PrintPoints(p, lenp);
				printf("     | ");
				for (i = 0; i < lenp; i++) {
					printf("%4d", i);
				}
				printf("    | ");
				for (i = 0; i < lenp; i++) {
					printf("%4d", s[i]);
				}
				printf("\n");

				// separator
				printf("-----+-");
				for (i = 0; i < lenp; i++) {
					printf("----");
				}
				printf("----+");
				for (i = 0; i < lenp; i++) {
					printf("----");
				}
				printf("-\n");
#endif /* DEBUG */

				// create a set of sequential indices
				// only count values that haven't failed the test
				//  i_???? = i0
				// 	i_prev = i1
				// 	i_curr = i2
				// 	i_next = i3
				i0 = 0;
				while (t[s[i0%lenp]] == CONCAVE) i0++;
				i1 = i0 + 1;
				while (t[s[i1%lenp]] == CONCAVE) i1++;
				i2 = i1 + 1;
				while (t[s[i2%lenp]] == CONCAVE) i2++;
				i3 = i2 + 1;
				while (t[s[i3%lenp]] == CONCAVE) i3++;
				imax = i2 + lenp;
				// go through the whole list once
				// check forward and backward at each failure
				// (until success in both directions)
#ifdef DEBUG
				printf("%4u | ", s[i2%lenp]);
				// test results, of this round
				for (i = 0; i < lenp; i++) {
					printf("%4u", t[i]);
				}
				printf("    | ");
				for (i = 0; i < lenp; i++) {
					printf("%4u", t[s[i]]);
				}
				printf("\n");
#endif /* DEBUG */
				while (i2 < imax) {

					// test v_1
					t[s[i2%lenp]] = ConvexityTest3D(vii, &v[s[i1%lenp] * 3], &v[s[i2%lenp] * 3], &v[s[i3%lenp] * 3]);

#ifdef DEBUG
					printf("%4u | ", s[i2%lenp]);
					// test results, of this round
					for (i = 0; i < lenp; i++) {
						printf("%4u", t[i]);
					}
					printf("    | ");
					for (i = 0; i < lenp; i++) {
						printf("%4u", t[s[i]]);
					}
					printf(" L\n");
#endif /* DEBUG */

					if (t[s[i2%lenp]] == CONCAVE) {
						// concavity detected

						// failsafe
						tcount++;
						if (tcount >= lenp) {
							break;
							break;
						}

						// increment forward
						i2 = i3;
						i3++;
						while (t[s[i3%lenp]] == CONCAVE) i3++;

						// check forward and backward until coast is clear
						chk1 = CONCAVE;
						chk2 = CONCAVE;
						while (chk1 == CONCAVE || chk2 == CONCAVE) {
							// check backward
							chk1 = ConvexityTest3D(vii, &v[s[i0%lenp] * 3], &v[s[i1%lenp] * 3], &v[s[i2%lenp] * 3]);
							if (chk1 == CONCAVE) {
								t[s[i1%lenp]] = CONCAVE;
								// failsafe
								tcount++;
								if (tcount >= lenp) {
									break;
									break;
									break;
								}
								i1 = i0;
								i0 += lenp - 1;
								while (t[s[i0%lenp]] == CONCAVE) i0 += lenp - 1;
							}
							// check forward
							chk2 = ConvexityTest3D(vii, &v[s[i1%lenp] * 3], &v[s[i2%lenp] * 3], &v[s[i3%lenp] * 3]);
							if (chk2 == CONCAVE) {
								t[s[i2%lenp]] = CONCAVE;
								// failsafe
								tcount++;
								if (tcount >= lenp) {
									break;
									break;
									break;
								}
								i2 = i3;
								i3++;
								while (t[s[i3%lenp]] == CONCAVE) i3++;
							}
#ifdef DEBUG
							printf("%4u | ", s[i2%lenp]);
							// test results, of this round
							for (i = 0; i < lenp; i++) {
								printf("%4u", t[i]);
							}
							printf("    | ");
							for (i = 0; i < lenp; i++) {
								printf("%4u", t[s[i]]);
							}
							printf(" F\n");
#endif /* DEBUG */
						}
						// concavity removed
					}
					else {
						// no concavity, continue
						i0 = i1;
						i1 = i2;
						i2 = i3;
						i3++;
						while (t[s[i3%lenp]] == CONCAVE) i3++;
					}
				}

#ifdef DEBUG
				printf("%4u | ", s[i2%lenp]);
				// test results, of this round
				for (i = 0; i < lenp; i++) {
					printf("%4u", t[i]);
				}
				printf("    | ");
				for (i = 0; i < lenp; i++) {
					printf("%4u", t[s[i]]);
				}
				printf(" \n");
#endif /* DEBUG */

				// failsafe
				if (tcount >= lenp) {
					break;
				}

				// create vertices, edges, facets
				if (listtype == VERTICES) {
					// VERTICES
					// list this vertex, push the rest to the queue (they'll get added on their turn
#ifdef DEBUG
					printf("DEBUG: VERTICES\n");
#endif /* DEBUG */
					l[lenl * 1 + 0] = ii;
					lenl++;
					for (i = 0; i < lenp; i++) {
						// push all unvisited points to the queue
						if (t[i] != CONCAVE && visited[i] == UNVISITED) {
							visited[i] = QUEUED;
							q[q2++] = i;
						}
					}
				}
				else if (listtype == EDGES) {
					// EDGES
					// list edges
#ifdef DEBUG
					printf("DEBUG: EDGES\n");
#endif /* DEBUG */
					for (i = 0; i < lenp; i++) {
						// if a point has been visited, the line already exisits
						// if a point is planar, it's line is unnecessary
						if (t[i] == CONVEX && visited[i] != VISITED) {
							l[lenl * 2 + 0] = ii;
							l[lenl * 2 + 1] = i;
							lenl++;
							if (visited[i] != QUEUED) {
								visited[i] = QUEUED;
								q[q2++] = i;
							}
						}
					}
				}
				else {
					// FACETS
					// create a triangle fan of points that pass
#ifdef DEBUG
					printf("DEBUG: FACETS\n");
#endif /* DEBUG */
					i1 = 0;
					while (t[s[i1%lenp]] == CONCAVE) i1++;
					i2 = i1 + 1;
					while (t[s[i2%lenp]] == CONCAVE) i2++;
					while (i1 < lenp) {
						// if a triangle contains a visited point, it's already in the list
						if (visited[s[i1%lenp]] != VISITED && visited[s[i2%lenp]] != VISITED) {
							// if planar, confirm that the normal is not yet disallowed
							chk1 = TRUE;
							if (t[s[i1%lenp]] == PLANAR || t[s[i2%lenp]] == PLANAR) {
								CrossProduct(&v[s[i1%lenp]], &v[s[i1%lenp]], u0);
								UnitVector(u0);
								for (i = 0; i < lenn1; i++) {
									if (abs(u0[0] - n[i * 3 + 0]) <= CH3D_EPSILON &&
										abs(u0[1] - n[i * 3 + 1]) <= CH3D_EPSILON &&
										abs(u0[2] - n[i * 3 + 2]) <= CH3D_EPSILON) {
										chk1 = FALSE;
										break;
									}
								}
								// if planar, but normal not disallowed
								// add point, and check if normal is listed
								// if not, add it
								chk2 = FALSE;
								for (i = lenn1; i < lenn2; i++) {
									if (abs(u0[0] - n[i * 3 + 0]) <= CH3D_EPSILON &&
										abs(u0[1] - n[i * 3 + 1]) <= CH3D_EPSILON &&
										abs(u0[2] - n[i * 3 + 2]) <= CH3D_EPSILON) {
										chk2 = TRUE;
										break;
									}
								}
								if (chk2 == FALSE) {
									n[lenn2 * 3 + 0] = u0[0];
									n[lenn2 * 3 + 1] = u0[1];
									n[lenn2 * 3 + 2] = u0[2];
									lenn2++;
								}
							}
							if (chk1 == TRUE) {
								l[lenl * 3 + 0] = ii;
								l[lenl * 3 + 1] = s[i1%lenp];
								l[lenl * 3 + 2] = s[i2%lenp];
								lenl++;
								if (visited[s[i1%lenp]] != QUEUED) {
									visited[s[i1%lenp]] = QUEUED;
									q[q2++] = s[i1%lenp];
								}
								if (visited[s[i2%lenp]] != QUEUED) {
									visited[s[i2%lenp]] = QUEUED;
									q[q2++] = s[i2%lenp];
								}
							}
						}
						i1 = i2;
						i2++;
						while (t[s[i2%lenp]] == CONCAVE) i2++;
					}
				}

				if (lenl > origLenl * sizeof(unsigned int))
				{
					throw 20;
				}

				// make any new normals off limits
				lenn1 = lenn2;
				// mark point as off limits
				visited[ii] = VISITED;
			}

#ifdef DEBUG
			if (tcount >= lenp) {
				printf("DEBUG: FAILSAFE ENGAGED\n");
				lenl = 0;
				leni = 0;
				free(l);
				l = NULL;
				*leni_p = leni;
				*lenl_p = lenl;
				*l_p = l;
				return;
			}
#endif /* DEBUG */

#ifdef DEBUG
			printf("DEBUG: FREE\n");
#endif /* DEBUG */
		}
		catch (int e) {
			throw;
		}
	}
	catch (...) {
		// Pfff.... Buggy convex hull library - overflow of buffers is possible...
		Sys_Printf("COLLISION: HULL GENERATION FAILED - convex hull library exception!\n");

		// free everything
		free(visited);
		free(q);
		free(v);
		free(theta);
		free(s);
		free(s2);
		free(t);
		free(n);

		leni = 0;
		lenl = 0;

		free(l);
		l = NULL;
		*leni_p = leni;
		*lenl_p = lenl;
		*l_p = l;

		free(p);
		p = NULL;

		return;
	}

	// free everything
	free(visited);
	free(q);
	free(v);
	free(theta);
	free(s);
	free(s2);
	free(t);
	free(n);

#ifdef DEBUG
	printf("DEBUG: REALLOC\n");
#endif /* DEBUG */

	// resize, in case of sub-optimal/non-convex set
	l = (unsigned int*)realloc(l, leni*lenl * sizeof(unsigned int));
	*leni_p = leni;
	*lenl_p = lenl;
	*l_p = l;

#ifdef DEBUG
	printf("DEBUG: END\n");
#endif /* DEBUG */
}

void ConvexHull3D(float** p_p, unsigned int* lenp_p, unsigned int** l_p, unsigned int* lenl_p, unsigned int* leni_p, int listtype) {
	float* p = NULL;
	unsigned int lenp = 0;

#ifdef DEBUG
	printf("DEBUG: ConvexHull3D BEGIN\n");
#endif /* DEBUG */

	// get rid of pesky repetitions get rid of pesky repetitions
	RemoveRepetitions(p_p, lenp_p);

	// get points and length
	p = *p_p;
	lenp = *lenp_p;

	// build it, whatever it is
	if (lenp == 1) {

		BuildPoint(p, lenp, l_p, lenl_p, leni_p, listtype);

	}
	else if (lenp == 2) {

		BuildLine(p, lenp, l_p, lenl_p, leni_p, listtype);

	}
	else if (LinearCheck(p, lenp) == TRUE) {

		BuildLinear(p, lenp, l_p, lenl_p, leni_p, listtype);

	}
	else if (lenp == 3) {

		BuildTriangle(p, lenp, l_p, lenl_p, leni_p, listtype);

	}
	else if (PlanarCheck(p, lenp) == TRUE) {

		BuildPlanar(p, lenp, l_p, lenl_p, leni_p, listtype);

	}
	else if (lenp == 4) {

		BuildTetrahedron(p, lenp, l_p, lenl_p, leni_p, listtype);

	}
	else {

		BuildHull(p, lenp, l_p, lenl_p, leni_p, listtype);

	}

#ifdef DEBUG
	printf("DEBUG: ConvexHull3D END\n");
#endif /* DEBUG */

	return;
}
