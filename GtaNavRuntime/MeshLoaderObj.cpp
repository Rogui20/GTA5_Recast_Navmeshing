//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "MeshLoaderObj.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <math.h>

rcMeshLoaderObj::rcMeshLoaderObj() :
	m_scale(1.0f),
	m_verts(0),
	m_tris(0),
	m_normals(0),
	m_vertCount(0),
	m_triCount(0)
{
}

rcMeshLoaderObj::~rcMeshLoaderObj()
{
	delete [] m_verts;
	delete [] m_normals;
	delete [] m_tris;
}
		
void rcMeshLoaderObj::addVertex(float x, float y, float z, int& cap)
{
	if (m_vertCount+1 > cap)
	{
		cap = !cap ? 8 : cap*2;
		float* nv = new float[cap*3];
		if (m_vertCount)
			memcpy(nv, m_verts, m_vertCount*3*sizeof(float));
		delete [] m_verts;
		m_verts = nv;
	}
	float* dst = &m_verts[m_vertCount*3];
	*dst++ = x*m_scale;
	*dst++ = y*m_scale;
	*dst++ = z*m_scale;
	m_vertCount++;
}

void rcMeshLoaderObj::addTriangle(int a, int b, int c, int& cap)
{
	if (m_triCount+1 > cap)
	{
		cap = !cap ? 8 : cap*2;
		int* nv = new int[cap*3];
		if (m_triCount)
			memcpy(nv, m_tris, m_triCount*3*sizeof(int));
		delete [] m_tris;
		m_tris = nv;
	}
	int* dst = &m_tris[m_triCount*3];
	*dst++ = a;
	*dst++ = b;
	*dst++ = c;
	m_triCount++;
}

static char* parseRow(char* buf, char* bufEnd, char* row, int len)
{
	bool start = true;
	bool done = false;
	int n = 0;
	while (!done && buf < bufEnd)
	{
		char c = *buf;
		buf++;
		// multirow
		switch (c)
		{
			case '\\':
				break;
			case '\n':
				if (start) break;
				done = true;
				break;
			case '\r':
				break;
			case '\t':
			case ' ':
				if (start) break;
				// else falls through
			default:
				start = false;
				row[n++] = c;
				if (n >= len-1)
					done = true;
				break;
		}
	}
	row[n] = '\0';
	return buf;
}

static int parseFace(char* row, int* data, int n, int vcnt)
{
	int j = 0;
	while (*row != '\0')
	{
		// Skip initial white space
		while (*row != '\0' && (*row == ' ' || *row == '\t'))
			row++;
		char* s = row;
		// Find vertex delimiter and terminated the string there for conversion.
		while (*row != '\0' && *row != ' ' && *row != '\t')
		{
			if (*row == '/') *row = '\0';
			row++;
		}
		if (*s == '\0')
			continue;
		int vi = atoi(s);
		data[j++] = vi < 0 ? vi+vcnt : vi-1;
		if (j >= n) return j;
	}
	return j;
}

bool rcMeshLoaderObj::load(const std::string& filename)
{
	char* buf = 0;
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp)
		return false;
	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);
		return false;
	}
	long bufSize = ftell(fp);
	if (bufSize < 0)
	{
		fclose(fp);
		return false;
	}
	if (fseek(fp, 0, SEEK_SET) != 0)
	{
		fclose(fp);
		return false;
	}
	buf = new char[bufSize];
	if (!buf)
	{
		fclose(fp);
		return false;
	}
	size_t readLen = fread(buf, bufSize, 1, fp);
	fclose(fp);

	if (readLen != 1)
	{
		delete[] buf;
		return false;
	}

	char* src = buf;
	char* srcEnd = buf + bufSize;
	char row[512];
	int face[32];
	float x,y,z;
	int nv;
	int vcap = 0;
	int tcap = 0;
	
	while (src < srcEnd)
	{
		// Parse one row
		row[0] = '\0';
		src = parseRow(src, srcEnd, row, sizeof(row)/sizeof(char));
		// Skip comments
		if (row[0] == '#') continue;
		if (row[0] == 'v' && row[1] != 'n' && row[1] != 't')
		{
			// Vertex pos
			sscanf(row+1, "%f %f %f", &x, &y, &z);
			addVertex(x, y, z, vcap);
		}
		if (row[0] == 'f')
		{
			// Faces
			nv = parseFace(row+1, face, 32, m_vertCount);
			for (int i = 2; i < nv; ++i)
			{
				const int a = face[0];
				const int b = face[i-1];
				const int c = face[i];
				if (a < 0 || a >= m_vertCount || b < 0 || b >= m_vertCount || c < 0 || c >= m_vertCount)
					continue;
				addTriangle(a, b, c, tcap);
			}
		}
	}

	delete [] buf;

	// Calculate normals.
	m_normals = new float[m_triCount*3];
	for (int i = 0; i < m_triCount*3; i += 3)
	{
		const float* v0 = &m_verts[m_tris[i]*3];
		const float* v1 = &m_verts[m_tris[i+1]*3];
		const float* v2 = &m_verts[m_tris[i+2]*3];
		float e0[3], e1[3];
		for (int j = 0; j < 3; ++j)
		{
			e0[j] = v1[j] - v0[j];
			e1[j] = v2[j] - v0[j];
		}
		float* n = &m_normals[i];
		n[0] = e0[1]*e1[2] - e0[2]*e1[1];
		n[1] = e0[2]*e1[0] - e0[0]*e1[2];
		n[2] = e0[0]*e1[1] - e0[1]*e1[0];
		float d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
		if (d > 0)
		{
			d = 1.0f/d;
			n[0] *= d;
			n[1] *= d;
			n[2] *= d;
		}
	}
	
	m_filename = filename;
	return true;
}


bool rcMeshLoaderObj::loadFromArrays(const float* verts, int nverts,
                                     const int* tris,  int ntris)
{
	// Limpa dados antigos
	delete [] m_verts;
	m_verts = 0;
	delete [] m_tris;
	m_tris = 0;
	delete [] m_normals;
	m_normals = 0;

	m_filename.clear();

	if (!verts || !tris || nverts <= 0 || ntris <= 0)
		return false;

	m_vertCount = nverts;
	m_triCount  = ntris;

	// copia vértices
	m_verts = new float[m_vertCount * 3];
	memcpy(m_verts, verts, sizeof(float) * m_vertCount * 3);

	// copia indices
	m_tris = new int[m_triCount * 3];
	memcpy(m_tris, tris, sizeof(int) * m_triCount * 3);

	// aloca normais (igual ao load normal)
	m_normals = new float[m_triCount * 3];

	// Calcula normais por triângulo (mesmo esquema do load())
	for (int i = 0; i < m_triCount * 3; i += 3)
	{
		const int* t = &m_tris[i];
		const float* v0 = &m_verts[t[0] * 3];
		const float* v1 = &m_verts[t[1] * 3];
		const float* v2 = &m_verts[t[2] * 3];

		float e1[3], e2[3], n[3];
		e1[0] = v1[0] - v0[0];
		e1[1] = v1[1] - v0[1];
		e1[2] = v1[2] - v0[2];
		e2[0] = v2[0] - v0[0];
		e2[1] = v2[1] - v0[1];
		e2[2] = v2[2] - v0[2];
		n[0] = e1[1]*e2[2] - e1[2]*e2[1];
		n[1] = e1[2]*e2[0] - e1[0]*e2[2];
		n[2] = e1[0]*e2[1] - e1[1]*e2[0];

		const float d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
		if (d > 0)
		{
			n[0] /= d;
			n[1] /= d;
			n[2] /= d;
		}

		m_normals[i + 0] = n[0];
		m_normals[i + 1] = n[1];
		m_normals[i + 2] = n[2];
	}

	return true;
}


bool rcMeshLoaderObj::initFromArrays(const float* verts, int nverts,
                                     const int* tris, int ntris)
{
    // limpar qualquer dado anterior
    delete[] m_verts;
    delete[] m_tris;
    delete[] m_normals;

    m_vertCount = nverts;
    m_triCount = ntris;

    // copiar vértices
    m_verts = new float[nverts * 3];
    memcpy(m_verts, verts, sizeof(float) * nverts * 3);

    // copiar índices
    m_tris = new int[ntris * 3];
    memcpy(m_tris, tris, sizeof(int) * ntris * 3);

    // calcular normais igual ao loader original
    m_normals = new float[ntris * 3];
    for (int i = 0; i < ntris * 3; i += 3)
    {
        const float* v0 = &m_verts[m_tris[i] * 3];
        const float* v1 = &m_verts[m_tris[i + 1] * 3];
        const float* v2 = &m_verts[m_tris[i + 2] * 3];
        float e0[3] = { v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2] };
        float e1[3] = { v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2] };

        float* n = &m_normals[i];
        n[0] = e0[1]*e1[2] - e0[2]*e1[1];
        n[1] = e0[2]*e1[0] - e0[0]*e1[2];
        n[2] = e0[0]*e1[1] - e0[1]*e1[0];
        float d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        if (d > 0) { n[0]/=d; n[1]/=d; n[2]/=d; }
    }

    // filename simbólico, já que não veio de arquivo
    m_filename = "dynamic_array_mesh";

    return true;
}

bool rcMeshLoaderObj::loadBIN(const std::string& fullPath)
{
    FILE* f = fopen(fullPath.c_str(), "rb");
    if (!f) return false;

    uint32_t magic = 0;
    uint32_t version = 0;

    fread(&magic,   sizeof(uint32_t), 1, f);
    fread(&version, sizeof(uint32_t), 1, f);

    if (magic != 0x4D4F424A) // 'MOBJ'
    {
        fclose(f);
        return false;
    }

    uint32_t vertCount = 0;
    uint32_t triCount  = 0;

    fread(&vertCount, sizeof(uint32_t), 1, f);
    m_vertCount = vertCount;

    m_verts = new float[vertCount * 3];
    fread(m_verts, sizeof(float), vertCount * 3, f);

    fread(&triCount, sizeof(uint32_t), 1, f);
    m_triCount = triCount;

    m_tris = new int[triCount * 3];
    fread(m_tris, sizeof(int), triCount * 3, f);

    // normais
    m_normals = new float[triCount * 3];
    fread(m_normals, sizeof(float), triCount * 3, f);

    fclose(f);
    return true;
}


// ======================================================================
//  FUNÇÃO: saveBIN()
// ======================================================================
bool rcMeshLoaderObj::saveBIN(const std::string& basePath)
{
    std::string fullPath = basePath + ".bin";
    FILE* f = fopen(fullPath.c_str(), "wb");
    if (!f) return false;

    uint32_t magic = 0x4D4F424A; // "MOBJ"
    uint32_t version = 1;

    fwrite(&magic,   sizeof(uint32_t), 1, f);
    fwrite(&version, sizeof(uint32_t), 1, f);

    fwrite(&m_vertCount, sizeof(uint32_t), 1, f);
    fwrite(m_verts, sizeof(float), m_vertCount * 3, f);

    fwrite(&m_triCount, sizeof(uint32_t), 1, f);
    fwrite(m_tris, sizeof(int), m_triCount * 3, f);

    fwrite(m_normals, sizeof(float), m_triCount * 3, f);

    fclose(f);
    return true;
}


// ======================================================================
//  FUNÇÃO: tryLoadBIN()
// ======================================================================
bool rcMeshLoaderObj::tryLoadBIN(const std::string& basePath)
{
    // 1) tenta BIN
    {
        const std::string binPath = basePath + ".bin";
        FILE* f = fopen(binPath.c_str(), "rb");

        if (f)
        {
            fclose(f);
            if (loadBIN(binPath))
            {
                printf("[MOBJ] Carregado BIN: %s\n", binPath.c_str());
                return true;
            }
        }
    }

    // 2) tenta OBJ (caminho base → adiciona .obj)
    const std::string objPath = basePath + ".obj";

    if (!load(objPath))
    {
        printf("[MOBJ] Falha OBJ: %s\n", objPath.c_str());
        return false;
    }

    // 3) salva BIN recém carregado
    saveBIN(basePath);
    printf("[MOBJ] OBJ carregado e BIN gerado: %s.bin\n", basePath.c_str());

    return true;
}