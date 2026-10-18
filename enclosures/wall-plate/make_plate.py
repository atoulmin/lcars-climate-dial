"""Australian-size wall plate for the Open Nextion ONX2424G013 knob.

The knob's rear housing (43.27 mm) sits in a cup through the plate and is held by
its three M1.6 nuts on posts inside the cup. The front ring (48 mm) is the part
that turns, so it is kept 1 mm proud of the plate face. A USB-C cable comes
straight out of the back into the wall cavity through the opening in the cup floor.

Coordinates: mm, z = 0 is the wall (plate back), +z points out of the wall,
x/y as seen from the front. Run: pip install -r ../requirements.txt, then python make_plate.py
"""
import math

import numpy as np
import trimesh
from manifold3d import CrossSection, JoinType, Manifold, set_circular_segments

set_circular_segments(128)

# Plate (standard Australian switch plate footprint, portrait)
PLATE_W, PLATE_H, PLATE_T = 73.0, 115.0, 4.0
PLATE_CORNER_R = 5.0
CHAMFER = 0.8                 # front edge
SCREW_CENTRES = 84.0          # vertical, as on wall boxes / mounting brackets
SCREW_HOLE_D = 4.2            # M3.5 plate screws
SCREW_HEAD_D = 7.5            # countersink for 90 degree heads

# Knob (from ONX2424G013-Dimension.pdf)
HOUSING_D = 43.27             # rear housing, fixed part
RING_D = 48.0                 # front ring, turns
HOUSING_DEPTH = 25.4 - 17.0   # rear housing body behind the front ring
CONNECTOR_DEPTH = 28.0 - 25.4 # connectors sticking out of the back
RING_GAP = 1.0                # turning ring clearance above the plate face
FIT = 0.7                     # diametral clearance for the housing in the cup
# M1.6 nut positions on the back, seen from the back (drawing bottom view)
NUTS_BACK_VIEW = [(-16.90, 0.0), (9.11, 14.72), (9.11, -14.42)]

# Cup
CUP_WALL = 2.0
FLOOR_T = 2.0
POST_D = 5.0
POST_H = CONNECTOR_DEPTH + 0.4         # clears the rear connectors
NUT_SCREW_D = 1.8                      # M1.6 clearance
NUT_HEAD_D, NUT_HEAD_DEPTH = 3.4, 1.2  # counterbore for M1.6 pan heads (from the back)
CABLE_OPENING_R = 14.0                 # USB-C plug and cable pass through here

housing_back_z = PLATE_T + RING_GAP - HOUSING_DEPTH
post_bottom_z = housing_back_z - POST_H
floor_bottom_z = post_bottom_z - FLOOR_T


def rounded_rect(w, h, r):
    return CrossSection.square([w - 2 * r, h - 2 * r], center=True).offset(r, JoinType.Round)


def cylinder(d, z0, z1, x=0.0, y=0.0):
    return Manifold.cylinder(z1 - z0, d / 2, d / 2).translate([x, y, z0])


def cone(d0, d1, z0, z1, x=0.0, y=0.0):
    return Manifold.cylinder(z1 - z0, d0 / 2, d1 / 2).translate([x, y, z0])


# Plate body with a chamfered front edge (two stacked extrusions)
body = rounded_rect(PLATE_W, PLATE_H, PLATE_CORNER_R).extrude(PLATE_T - CHAMFER)
front = Manifold.extrude(rounded_rect(PLATE_W, PLATE_H, PLATE_CORNER_R), CHAMFER, scale_top=[
    (PLATE_W - 2 * CHAMFER) / PLATE_W, (PLATE_H - 2 * CHAMFER) / PLATE_H]).translate([0, 0, PLATE_T - CHAMFER])
plate = body + front

# Cup tube behind the plate, floor and posts
cup_in_d = HOUSING_D + FIT
cup_out_d = cup_in_d + 2 * CUP_WALL
plate += cylinder(cup_out_d, floor_bottom_z, 0.01)
plate -= cylinder(cup_in_d, post_bottom_z, PLATE_T + 1)

# Nut positions seen from the front are mirrored left-right
nuts = [(-x, y) for x, y in NUTS_BACK_VIEW]
for x, y in nuts:
    plate += cylinder(POST_D, post_bottom_z - 0.01, housing_back_z, x, y)

# Cable opening, screw holes and counterbores
plate -= cylinder(CABLE_OPENING_R * 2, floor_bottom_z - 1, post_bottom_z + 0.01)
for x, y in nuts:
    plate -= cylinder(NUT_SCREW_D, floor_bottom_z - 1, housing_back_z + 1, x, y)
    plate -= cylinder(NUT_HEAD_D, floor_bottom_z - 1, floor_bottom_z + NUT_HEAD_DEPTH, x, y)

# Small chamfer on the cup mouth
plate -= cone(cup_in_d, cup_in_d + 1.6, PLATE_T - 0.8, PLATE_T + 0.001)

# Plate screws, countersunk from the front
for y in (-SCREW_CENTRES / 2, SCREW_CENTRES / 2):
    plate -= cylinder(SCREW_HOLE_D, -1, PLATE_T + 1, 0, y)
    sink = (SCREW_HEAD_D - SCREW_HOLE_D) / 2
    plate -= cone(SCREW_HOLE_D, SCREW_HEAD_D, PLATE_T - sink, PLATE_T + 0.001, 0, y)

mesh = plate.to_mesh()
tm = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts))
assert tm.is_watertight
tm.export("onx2424g013_au_wallplate.stl")

print(f"plate {PLATE_W} x {PLATE_H} x {PLATE_T} mm, cup sticks {-floor_bottom_z:.1f} mm into the wall")
print(f"turning ring sits {RING_GAP} mm above the plate face; M1.6 screws ~{FLOOR_T + POST_H - NUT_HEAD_DEPTH + 2:.0f} mm long")
print(f"bounds {tm.bounds.round(2).tolist()}, volume {tm.volume / 1000:.1f} cm3")

# Preview renders
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

fig = plt.figure(figsize=(12, 6), facecolor="black")
for i, (elev, azim, title) in enumerate([(35, -60, "front"), (-35, -120, "back")]):
    ax = fig.add_subplot(1, 2, i + 1, projection="3d", facecolor="black")
    tris = tm.vertices[tm.faces]
    normals = tm.face_normals
    light = np.array([0.4, -0.5, 0.8] if title == "front" else [0.4, 0.5, -0.8])
    shade = np.clip(normals @ (light / np.linalg.norm(light)), 0.15, 1)
    colors = np.c_[shade * 1.0, shade * 0.6, shade * 0.1, np.ones_like(shade)]
    ax.add_collection3d(Poly3DCollection(tris, facecolors=colors, linewidths=0))
    ax.set_xlim(-60, 60); ax.set_ylim(-60, 60); ax.set_zlim(-60, 60)
    ax.view_init(elev, azim)
    ax.set_axis_off()
    ax.set_title(title, color="white")
plt.tight_layout()
plt.savefig("preview.png", dpi=110, facecolor="black")
