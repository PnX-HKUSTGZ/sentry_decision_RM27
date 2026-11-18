from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    share = FindPackageShare("sentry_decision_bringup")
    config = LaunchConfiguration("config")
    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "config",
                default_value=PathJoinSubstitution([share, "config", "profiles.yaml"]),
                description="profiles.yaml 路径；2027 标定后传 profiles_rmuc27.yaml",
            ),
            Node(
                package="sentry_decision_bringup",
                executable="task_node",
                arguments=["--config", config],
                output="screen",
            ),
        ]
    )
