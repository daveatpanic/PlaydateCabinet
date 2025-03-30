
## lib3d - a 3D rendering Lua extension for Playdate

The lib3D extension is a 3D scene rendered, and an example of integrating C code into Lua. It implements a simple tree-based scene with triangle- or quad-faced shapes at the leaf nodes. By default there is no depth sorting; entire shapes are rendered one at a time using the painter's algorithm and backface culling to eliminate hidden faces, but a a buffer or an ordering table can be included through compile-time defines. No clipping is performed, either between shapes/faces or a clipping plane--if any of the vertices in a face is behind the camera, the face isn't rendered. Draw styles can be set per tree branch, to enable/disable filled faces (fill patterns are currently hardcoded in the source) and wireframe edges.

**Compile-time options (see mini3d.h):**

`#define ENABLE_Z_BUFFER 1`

Implements a 16-bit depth buffer. Use this when you have intersecting geometry.

`#define ENABLE_ORDERING_TABLE 1`

Ordering tables are a quick and dirty way to sort faces in a shape so it draws correctly via the painter's algorithm (rendering faces from back to front). Generally, you'd start with a low table size (see shape:setOrderTableSize()) and increase it until the shape is drawn correctly.


### lib3d.scene

**`lib3d.scene.new()`**

Creates a new 3D scene

**`lib3d.scene:draw()`**

Draws the scene into the current frame buffer

**`lib3d.scene:drawNode(n)`**

Draws the given node into the current frame buffer

**`lib3d.scene:getRootNode()`**

Returns the root node of the render tree, a lib3d.scenenode object

**`lib3d.scene:setCenter(x, y)`**

Sets the position of the distant vanishing point, in fractional screen coordinates. The default is (0.5, 0.5), meaning as an object recedes into the distance along the Z axis (when the camera target is the default (0, 0, 1) vector) it moves to the center of the screen

**`lib3d.scene:setCameraOrigin(x, y, z)`**

Sets the position of the camera

**`lib3d.scene:setCameraScale(s)`**

Sets the final scaling factor of the camera, akin to focal length. At default value 1.0, the extents of the screen (with default center) are [-1.6,1.6] x [-1,1].

**`lib3d.scene:setCameraTarget(x, y, z)`**

Points the camera at point (x,y,z) in 3D space

**`lib3d.scene:setCameraUp(x, y, z)`**

Rolls the camera so that vector (x,y,z) is pointing up

**`lib3d.scene:setLight(x, y, z)`**

Sets the direction of the global light source


### lib3d.scenenode

**`lib3d.scenenode:addChildNode()`**

Adds (and returns) a child node to the given scene node

**`lib3d.scenenode:addShape(s, [m])`**  
**`lib3d.scenenode:addShape(s, [x, y, z])`**

Adds the given libd3.shape to the node, with optional offset from the center of the node (x,y,z) or transformation matrix m.

**`lib3d.scenenode:addTransform(m)`**

Appends the given lib3d.matrix to the node's transform matrix

**`lib3d.scenenode:setTransform(m)`**

Replaces the node's transformation matrix with the given lib3d.matrix

**`lib3d.scenenode:translateBy(dx,dy,dz)`**

Appends a translation to the node's transform matrix

**`lib3d.scenenode:scaleBy(sx [,sy,sz])`**

Appends a scaling to the node's transform matrix. If only one argument is given, all axes are scaled by that amount.

**`lib3d.scenenode:setColorBias(bias)`**

Adds the given color bias to the node, lightening or darkening all shapes in the render tree underneath this node

**`lib3d.scenenode:setFilled(flag)`**

Turns face fills on or off. Default is filled.

**`lib3d.scenenode:setWireframeMode(mode)`**

If mode is 0 (default), no wireframe is drawn. If mode is 1, only the front-facing edges are drawn. If mode is 2, all edges are drawn.

**`lib3d.scenenode:setWireframeColor(color)`**

Sets the color of the edge drawing; 1 = white, 0 = black

**`lib3d.scenenode:setVisible(flag)`**

Turns rendering on and off for this branch of the render tree. Default is on, natch.


### lib3d.matrix

**`lib3d.matrix.new(m00, m01, m02, m10, m11, m12, m20, m21, m22, dx, dy, dz)`**

Returns a 3D affine transformation matrix with the given parameters

**`lib3d.matrix.newRotation(angle, dx, dy, dz)`**

Returns a transformation matrix representing a rotation of angle degrees around the vector (dx,dy,dx)

**`lib3d.matrix:addTranslation(dx, dy, dz)`**

Adds the given translation to the matrix

**`lib3d.matrix:__mul(m)`**

Metamethod allowing matrix composition with the * operator


### lib3d.point

**`lib3d.point.new(x, y, z)`**

Returns an object representing the point (x,y,z), used in constructing shapes

**`lib3d.point:__index(k)`**  
**`lib3d.point:__newindex(k,v)`**

Metamethod providing get and set for the point's x, y, and z coordinates

**`lib3d.point:__mul(m)`**

Metamethod for multiplying the point my matrix m


### lib3d.shape

**`lib3d.shape.new()`**

Creates a new shape object. Shapes are added to any number of lib3d.scenenode objects via the lib3d.scenenode:addShape() function.

**`lib3d.shape:addFace(p1, p2,  p3, [p4], [colorBias])`**

Adds a new triangular or quadrilateral face to the shape, with points p1, p2, p3 as the triangle's vertices or p1, p2, p3, and p4 for the quad. Note: order matters! If the shape is marked closed, back faces will be culled during drawing so the vertices must be given in counter-clockwise order, as seen from outside the shape. The optional colorBias argument is used to make the face brighter or darker.

**`lib3d.shape:setClosed(flag)`**

Marks the shape closed, allowing the renderer to skip faces with normals pointing away from the camera.

**`lib3d.shape:setOrderTableSize(bins)`**

Only available when compiled with the ENABLE_ORDERING_TABLE compile-time option set. Renders the shape using the given ordering table size.
