from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    share = FindPackageShare("sentry_decision_viz")
    rosbridge_port = LaunchConfiguration("rosbridge_port")
    http_port = LaunchConfiguration("http_port")
    return LaunchDescription(
        [
            DeclareLaunchArgument("rosbridge_port", default_value="9090"),
            DeclareLaunchArgument("http_port", default_value="8080"),
            Node(
                package="rosbridge_server",
                executable="rosbridge_websocket",
                parameters=[{"port": rosbridge_port}],
                output="screen",
            ),
            ExecuteProcess(
                cmd=[
                    "python3",
                    "-m",
                    "http.server",
                    http_port,
                    "--directory",
                    PathJoinSubstitution([share, "web"]),
                ],
                output="screen",
            ),
        ]
    )
