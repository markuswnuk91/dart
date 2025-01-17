import dartpy as dart
import numpy as np


class MyWorldNode(dart.gui.RealTimeWorldNode):
    def __init__(self, world, shadow):
        super(MyWorldNode, self).__init__(world, shadow)

    def customPreStep(self):
        pass


def main():
    world = dart.io.SkelParser.readWorld("dart://sample/skel/cubes.skel")
    world.setGravity([0, -9.81, 0])

    viewer = dart.gui.Viewer()
    shadow = dart.gui.WorldNode.createDefaultShadowTechnique(viewer)

    node = MyWorldNode(world, shadow)
    viewer.addWorldNode(node)

    viewer.setUpViewInWindow(0, 0, 640, 480)
    viewer.setCameraHomePosition([0.8, 0.0, 0.8], [0, -0.25, 0], [0, 0.5, 0])
    viewer.run()


if __name__ == "__main__":
    main()
