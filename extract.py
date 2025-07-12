import sys

import xml.etree.ElementTree as ET

def parse_svg_circles(svg_path):
    import re
    import math
    tree = ET.parse(svg_path)
    root = tree.getroot()
    ns = {'svg': 'http://www.w3.org/2000/svg'}
    circles = []

    def parse_transform(transform):
        # Returns a 3x3 matrix for 2D affine transforms
        if not transform:
            return [[1,0,0],[0,1,0],[0,0,1]]
        matrix = [[1,0,0],[0,1,0],[0,0,1]]
        for cmd, args in re.findall(r'(\w+)\(([^)]*)\)', transform):
            values = list(map(float, re.findall(r'-?\d*\.?\d+(?:[eE][-+]?\d+)?', args)))
            if cmd == 'translate':
                tx = values[0]
                ty = values[1] if len(values) > 1 else 0
                m = [[1,0,tx],[0,1,ty],[0,0,1]]
            elif cmd == 'scale':
                sx = values[0]
                sy = values[1] if len(values) > 1 else sx
                m = [[sx,0,0],[0,sy,0],[0,0,1]]
            elif cmd == 'rotate':
                a = math.radians(values[0])
                cos_a = math.cos(a)
                sin_a = math.sin(a)
                if len(values) == 3:
                    cx, cy = values[1], values[2]
                    m = [
                        [cos_a, -sin_a, cx-cos_a*cx+sin_a*cy],
                        [sin_a, cos_a, cy-sin_a*cx-cos_a*cy],
                        [0,0,1]
                    ]
                else:
                    m = [[cos_a,-sin_a,0],[sin_a,cos_a,0],[0,0,1]]
            elif cmd == 'matrix':
                a,b,c,d,e,f = values
                m = [[a,c,e],[b,d,f],[0,0,1]]
            else:
                continue
            # Matrix multiply: matrix = matrix @ m
            matrix = [
                [sum(matrix[i][k]*m[k][j] for k in range(3)) for j in range(3)]
                for i in range(3)
            ]
        return matrix

    def get_combined_transform(elem):
        # Walk up the tree to combine transforms
        transform = None
        matrices = []
        while elem is not None:
            t = elem.get('transform')
            if t:
                matrices.append(parse_transform(t))
            elem = elem.getparent() if hasattr(elem, 'getparent') else elem.find('..')
        # Multiply in reverse order (root first)
        result = [[1,0,0],[0,1,0],[0,0,1]]
        for m in reversed(matrices):
            result = [
                [sum(result[i][k]*m[k][j] for k in range(3)) for j in range(3)]
                for i in range(3)
            ]
        return result

    # Patch: ElementTree does not support getparent(), so we need to build a parent map
    parent_map = {c: p for p in root.iter() for c in p}

    for elem in root.findall('.//svg:circle', ns):
        cx = elem.get('cx')
        cy = elem.get('cy')
        if cx is not None and cy is not None:
            try:
                x = float(cx)
                y = float(cy)
                # Get combined transform
                e = elem
                matrices = []
                while e is not None:
                    t = e.get('transform')
                    if t:
                        matrices.append(parse_transform(t))
                    e = parent_map.get(e)
                # Multiply in reverse order (root first)
                m = [[1,0,0],[0,1,0],[0,0,1]]
                for mat in reversed(matrices):
                    m = [
                        [sum(m[i][k]*mat[k][j] for k in range(3)) for j in range(3)]
                        for i in range(3)
                    ]
                # Apply transform
                x_new = m[0][0]*x + m[0][1]*y + m[0][2]
                y_new = m[1][0]*x + m[1][1]*y + m[1][2]
                circles.append((x_new, y_new))
            except ValueError:
                continue
    return circles

def write_c_arrays(coords, number):
    xs = ', '.join(f'{x:.6f}' for x, _ in coords)
    ys = ', '.join(f'{y:.6f}' for _, y in coords)
    print(f'static constexpr float circle_xs_{number}[] = {{{xs}}};')
    print(f'static constexpr float circle_ys_{number}[] = {{{ys}}};')
    print(f'static constexpr int num_circles_{number} = {len(coords)};')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} <svg_file>')
        sys.exit(1)
    svg_file = sys.argv[1]
    number = int(sys.argv[2])
    coords = parse_svg_circles(svg_file)
    write_c_arrays(coords, number)