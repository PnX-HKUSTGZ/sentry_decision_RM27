from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    tree = PathJoinSubstitution(
        [FindPackageShare("sentry_decision_bringup"), "tree", "demo_tree.xml"]
    )
    return LaunchDescription(
        [
            Node(
                package="sentry_decision_bringup",
                executable="decision_node",
                arguments=["--tree", tree],
                output="screen",
            ),
        ]
    )
