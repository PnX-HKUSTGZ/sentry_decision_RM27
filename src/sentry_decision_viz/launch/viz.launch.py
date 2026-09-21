from ament_index_python.packages import PackageNotFoundError, get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def _require_rosbridge(context):
    # 缺失依赖时给出可操作的提示，而不是让 Node 抛找不到包。
    try:
        get_package_share_directory("rosbridge_server")
    except PackageNotFoundError as error:
        raise RuntimeError(
            "缺少 rosbridge_server：请在镜像中安装 ros-jazzy-rosbridge-suite"
            "（见 docker/Dockerfile），或改用已含该依赖的镜像。"
        ) from error
    return []


def generate_launch_description():
    share = FindPackageShare("sentry_decision_viz")
    rosbridge_port = LaunchConfiguration("rosbridge_port")
    http_port = LaunchConfiguration("http_port")
    return LaunchDescription(
        [
            DeclareLaunchArgument("rosbridge_port", default_value="9090"),
            DeclareLaunchArgument("http_port", default_value="8080"),
            OpaqueFunction(function=_require_rosbridge),
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
