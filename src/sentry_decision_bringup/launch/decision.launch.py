from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    share = FindPackageShare("sentry_decision_bringup")
    tree = PathJoinSubstitution([share, "tree", "root.xml"])
    config = PathJoinSubstitution([share, "config", "profiles.yaml"])
    return LaunchDescription(
        [
            ExecuteProcess(
                cmd=[
                    "decision_main",
                    "--ticks",
                    "200",
                    "--rate",
                    "20",
                    "--tree",
                    tree,
                    "--config",
                    config,
                ],
                output="screen",
            ),
        ]
    )
