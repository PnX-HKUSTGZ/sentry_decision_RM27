from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    tree = PathJoinSubstitution(
        [FindPackageShare("sentry_decision_bringup"), "tree", "demo_tree.xml"]
    )
    return LaunchDescription(
        [
            ExecuteProcess(
                cmd=["decision_main", "--ticks", "200", "--rate", "20", "--tree", tree],
                output="screen",
            ),
        ]
    )
