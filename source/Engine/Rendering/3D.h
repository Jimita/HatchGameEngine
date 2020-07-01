#ifndef I3D_H
#define I3D_H

class IVertex {
    public:
    float x;
    float y;
    float z;

    IVertex() {
        x = 0.0;
        y = 0.0;
        z = 0.0;
    }
    IVertex(float x, float y, float z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    IVertex Multiply(float x, float y, float z) {
        this->x *= x;
        this->y *= y;
        this->z *= z;
        return IVertex(this->x, this->y, this->z);
    }
    IVertex Multiply(IVertex v) {
        return Multiply(v.x, v.y, v.z);
    }

    float  DotProduct(float x2, float y2, float z2) {
        return x * x2 + y * y2 + z * z2;
    }
    float  DotProduct(IVertex v) {
        return x * v.x + y * v.y + z * v.z;
    }

    IVertex Normalize() {
        float len = sqrt(x * x + y * y + z * z);
        x /= len;
        y /= len;
        z /= len;
        return IVertex(x, y, z);
    }
    float  Distance() {
        return sqrt(x * x + y * y + z * z);
    }
};

class IVertex2 {
    public:
    float x;
    float y;

    IVertex2() {
        x = 0.0;
        y = 0.0;
    }
    IVertex2(float x, float y) {
        this->x = x;
        this->y = y;
    }

    IVertex2 Multiply(float x, float y) {
        this->x *= x;
        this->y *= y;
        return IVertex2(this->x, this->y);
    }
    IVertex2 Multiply(IVertex2 v) {
        return Multiply(v.x, v.y);
    }

    float  DotProduct(float x2, float y2) {
        return x * x2 + y * y2;
    }
    float  DotProduct(IVertex2 v) {
        return x * v.x + y * v.y;
    }

    IVertex2 Normalize() {
        float len = sqrt(x * x + y * y);
        x /= len;
        y /= len;
        return IVertex2(x, y);
    }
    float  Distance() {
        return sqrt(x * x + y * y);
    }
};

class IColor {
    public:
    float r;
    float g;
    float b;
    float a;

    IColor() {
        r = 1.0;
        g = 1.0;
        b = 1.0;
        a = 1.0;
    }
    IColor(float r, float g, float b) {
        this->r = r;
        this->g = g;
        this->b = b;
        this->a = 1.0f;
    }
    IColor(float r, float g, float b, float a) {
        this->r = r;
        this->g = g;
        this->b = b;
        this->a = a;
    }
};

class ITexCoord {
    public:
    float u;
    float v;

    ITexCoord() {
        u = 1.0;
        v = 1.0;
    }
    ITexCoord(float u, float v) {
        this->u = u;
        this->v = v;
    }
};

class IFace {
    public:
    int Vertices[3];
    int TexCoords[3];
    int Normals[3];
    int Colors[3];

    IFace() {}
    IFace(int v1, int v2, int v3) {
        this->Vertices[0] = v1;
        this->Vertices[1] = v2;
        this->Vertices[2] = v3;
    }
    IFace(int v1, int v2, int v3, int t1, int t2, int t3) {
        this->Vertices[0] = v1;
        this->Vertices[1] = v2;
        this->Vertices[2] = v3;
        this->TexCoords[0] = t1;
        this->TexCoords[1] = t2;
        this->TexCoords[2] = t3;
    }
    IFace(int v1, int v2, int v3, int t1, int t2, int t3, int n1, int n2, int n3) {
        this->Vertices[0] = v1;
        this->Vertices[1] = v2;
        this->Vertices[2] = v3;
        this->TexCoords[0] = t1;
        this->TexCoords[1] = t2;
        this->TexCoords[2] = t3;
        this->Normals[0] = n1;
        this->Normals[1] = n2;
        this->Normals[2] = n3;
    }
    IFace(int v1, int v2, int v3, int t1, int t2, int t3, int n1, int n2, int n3, int c1, int c2, int c3) {
        this->Vertices[0] = v1;
        this->Vertices[1] = v2;
        this->Vertices[2] = v3;
        this->TexCoords[0] = t1;
        this->TexCoords[1] = t2;
        this->TexCoords[2] = t3;
        this->Normals[0] = n1;
        this->Normals[1] = n2;
        this->Normals[2] = n3;
        this->Colors[0] = c1;
        this->Colors[1] = c2;
        this->Colors[2] = c3;
    }
};

class IMaterial {
    public:
    float Specular[4];
    float Ambient[4];
    float Emission[4];
    float Diffuse[4];

    float Shininess;
    float Transparency;
    float IndexOfRefraction;

    void* Tex;

    IMaterial() {
        for (int i = 0; i < 4; i++)
        {
            Specular[i] = 1.0f;
            Ambient[i] = 1.0f;
            Emission[i] = 1.0f;
            Diffuse[i] = 1.0f;
        }

        Shininess = 0.0f;
        Transparency = 1.0f;
        IndexOfRefraction = 1.0f;

        Tex = nullptr;
    }
};

class IMatrix3 {
    public:
    float Values[9];

    IMatrix3(float values[9]) {
        memcpy(Values, values, 9 * sizeof(float));
    }
    IMatrix3(float v1, float v2, float v3, float v4, float v5, float v6, float v7, float v8, float v9) {
        Values[0] = v1;
        Values[1] = v2;
        Values[2] = v3;

        Values[3] = v4;
        Values[4] = v5;
        Values[5] = v6;

        Values[6] = v7;
        Values[7] = v8;
        Values[8] = v9;
    }

    IMatrix3 Multiply(IMatrix3 other) {
        float result[9];
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                result[row * 3 + col] = 0;
                for (int i = 0; i < 3; i++) {
                    result[row * 3 + col] += this->Values[row * 3 + i] * other.Values[i * 3 + col];
                }
            }
        }
        return IMatrix3(result);
    }
    IVertex Transform(IVertex in) {
        return IVertex(
            in.x * Values[0] + in.y * Values[3] + in.z * Values[6],
            in.x * Values[1] + in.y * Values[4] + in.z * Values[7],
            in.x * Values[2] + in.y * Values[5] + in.z * Values[8]
        );
    }
};

class IMatrix2x3 {
    public:
    float Values[6];

    IMatrix2x3() {
        memset(Values, 0, 6 * sizeof(float));
    }
    IMatrix2x3(float values[6]) {
        memcpy(Values, values, 6 * sizeof(float));
    }
    IMatrix2x3(float v1, float v2, float v3, float v4, float v5, float v6) {
        Values[0] = v1;
        Values[1] = v2;
        Values[2] = v3;

        Values[3] = v4;
        Values[4] = v5;
        Values[5] = v6;
    }

    IMatrix2x3 Multiply(IMatrix2x3 other) {
        return other;
        /*
        float result[9];
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 2; col++) {
                result[row * 2 + col] = 0;
                for (int i = 0; i < 3; i++) {
                    result[row * 2 + col] += this->Values[row * 2 + i] * other.Values[i * 3 + col];
                }
            }
        }
        return IMatrix2x3(result);
        */
    }
    IVertex2 Transform(IVertex in) {
        return IVertex2(
            in.x * Values[0] + in.y * Values[1] + in.z * Values[2],
            in.x * Values[3] + in.y * Values[4] + in.z * Values[5]
        );
    }
    void SetColumn(int col, IVertex2 in) {
        Values[col] = in.x;
        Values[col + 3] = in.y;
    }
};

#endif /* I3D_H */
