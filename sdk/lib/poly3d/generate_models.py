#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Author small convex meshes and emit data, never prerendered frames."""
import argparse
import json
import math
from pathlib import Path

MODELS = {
    'tetra': {
        'vertices': [(-18, -10, -12), (18, -10, -12), (0, -10, 24), (0, 20, 0)],
        'faces': [(0, 1, 2), (0, 3, 1), (1, 3, 2), (2, 3, 0)],
    },
    'octa': {
        'vertices': [(0, 22, 0), (0, -22, 0), (-20, 0, 0), (20, 0, 0),
                     (0, 0, 20), (0, 0, -20)],
        'faces': [(0, 2, 4), (0, 4, 3), (0, 3, 5), (0, 5, 2),
                  (1, 4, 2), (1, 3, 4), (1, 5, 3), (1, 2, 5)],
    },
    'cube': {
        'vertices': [(-16, -16, -16), (16, -16, -16), (16, 16, -16), (-16, 16, -16),
                     (-16, -16, 16), (16, -16, 16), (16, 16, 16), (-16, 16, 16)],
        'faces': [(0, 1, 2), (0, 2, 3), (4, 7, 6), (4, 6, 5),
                  (0, 4, 5), (0, 5, 1), (3, 2, 6), (3, 6, 7),
                  (0, 3, 7), (0, 7, 4), (1, 5, 6), (1, 6, 2)],
    },
}


def meshes():
    result = {}
    for name, model in MODELS.items():
        vertices = model['vertices']
        assert 3 <= len(vertices) <= 8 and 1 <= len(model['faces']) <= 12
        assert all(-24 <= c <= 24 for v in vertices for c in v)
        center = [sum(v[i] for v in vertices) / len(vertices) for i in range(3)]
        faces = []
        for indices in model['faces']:
            a, b, c = [vertices[i] for i in indices]
            u, v = [[p[i] - a[i] for i in range(3)] for p in (b, c)]
            normal = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
            if sum(normal[i]*(a[i]-center[i]) for i in range(3)) < 0:
                indices = (indices[0], indices[2], indices[1])
                normal = [-n for n in normal]
            assert all(sum(normal[i]*(p[i]-a[i]) for i in range(3)) <= 0 for p in vertices), 'Nonconvex mesh'
            length = math.sqrt(sum(n*n for n in normal))
            assert length > 0
            faces.append([*indices, *[round(n*96/length) for n in normal], 255])
        result[name] = {'vertices': vertices, 'faces': faces}
    return result


def sources():
    data = meshes()
    asm = ['; SPDX-License-Identifier: MIT', '; Generated original convex mesh data. No frame images.',
           '.section .data, data']
    for name, model in data.items():
        prefix = 'p3_model_' + name
        asm += ['.global ' + prefix, prefix + ':',
                f'    .byte {len(model["vertices"])},{len(model["faces"])}',
                f'    .word {prefix}_vertices,{prefix}_faces', prefix + '_vertices:']
        for vertex in model['vertices']:
            asm.append('    .byte ' + ','.join(str(c & 255) for c in vertex))
        asm += [prefix + '_faces:']
        for face in model['faces']:
            asm.append('    .byte ' + ','.join(str(c & 255) for c in face))
    return {'models.s': '\n'.join(asm) + '\n', 'models.json': json.dumps(data, indent=2) + '\n'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for name, content in sources().items():
        path = Path(__file__).with_name(name)
        if args.check:
            if path.read_text() != content:
                raise SystemExit('Stale generated mesh: ' + name)
        else:
            path.write_text(content)
    print('Verified 3 original convex meshes.' if args.check else 'Generated 3 original convex meshes.')
