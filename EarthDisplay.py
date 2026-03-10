import plotly.graph_objects as go
import numpy as np
import math

slices_number = 32
leds_per_slice = 20

from tifffile import imread

numpy_data = imread('elevation_100KMmd_GMTEDmd.tif')

x=[]
y=[]
z=[]
for xid, xdata in enumerate(numpy_data):
    for yid, height in enumerate(xdata):
        if height > 0:
            x.append(xid)
            y.append(yid)
            z.append(height/1000)

fig = go.Figure(data=[go.Scatter3d(
    x=x,
    y=y,
    z=z,
    mode='markers',
    marker=dict(
        size=5,
        color=height,
        colorscale='Viridis',
        opacity=0.8
    )
)])

# Set layout for better visualization
fig.update_layout(
    scene=dict(
        xaxis_title='X',
        yaxis_title='Y',
        zaxis_title='Z',
        aspectmode='data'
    ),
    title='Interactive 3D Scatter Plot of Spherical Coordinates'
)

fig.show()


# # Generate points:
# points = []
# x = []
# y = []
# z = []
# r = 1  # radius of the sphere

# longitude = 0
# for slice in range(slices_number):
#     latitude = 0
#     for led in range(leds_per_slice):

#         # Convert to Cartesian coordinates
#         x.append(r * np.sin(math.radians(latitude)) * np.cos(math.radians(longitude)))
#         y.append(r * np.sin(math.radians(latitude)) * np.sin(math.radians(longitude)))
#         z.append(r * np.cos(math.radians(latitude)))

#         latitude += 180 / leds_per_slice

#     longitude += 360/ slices_number

# # Create 3D scatter plot
# fig = go.Figure(data=[go.Scatter3d(
#     x=x,
#     y=y,
#     z=z,
#     mode='markers',
#     marker=dict(
#         size=5,
#         color=r,  # color by radius
#         colorscale='Viridis',
#         opacity=0.8
#     )
# )])

# # Set layout for better visualization
# fig.update_layout(
#     scene=dict(
#         xaxis_title='X',
#         yaxis_title='Y',
#         zaxis_title='Z',
#     ),
#     title='Interactive 3D Scatter Plot of Spherical Coordinates'
# )

# fig.show()