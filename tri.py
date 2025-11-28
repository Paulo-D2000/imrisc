import matplotlib.pyplot as plt
import numpy as np

# Define the triangles
vertices = [
    (80, 40),
    (120, 80),
    (40, 80),
    
    (40, 80),
    (120, 80),
    (80, 120),
]

vertices = vertices + [(i[0]+80, i[1]) for i in vertices]

vertices = vertices + [
    (80, 120),
    (160, 120),
    (120, 160),

    (80, 120),
    (120, 80),
    (160, 120),
]

print('{\n' + ',\n'.join('{{%d,%d}}'%(v[0], v[1]) for v in vertices) + '\n};')

# Convert to numpy array
vertices = np.array(vertices)

# Plot each triangle
fig, ax = plt.subplots()
for i in range(0, len(vertices), 3):
    tri = vertices[i:i+3]
    polygon = plt.Polygon(tri, closed=True, edgecolor='black', facecolor='red', alpha=0.6)
    ax.add_patch(polygon)

# Adjust plot limits
ax.set_xlim(0, 320)
ax.set_ylim(0, 240)
ax.set_aspect('equal')
plt.grid(True)
plt.gca().invert_yaxis()
plt.show()