from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    share = FindPackageShare("sentry_decision_bringup")
    tree = PathJoinSubstitution([share, "tree", "root.xml"])
    config = PathJoinSubstitution([share, "config", "profiles.yaml"])
    return LaunchDescription(
        [
            Node(
                package="sentry_decision_bringup",
                executable="decision_node",
                arguments=["--tree", tree, "--config", config],
                output="screen",
            ),
        ]
    )
