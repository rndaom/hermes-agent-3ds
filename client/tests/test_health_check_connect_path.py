from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_connect_paths_verify_so_error_after_connect_wait():
    health_source = (CLIENT_DIR / "source" / "bridge_health.c").read_text()
    v2_source = (CLIENT_DIR / "source" / "bridge_v2.c").read_text()

    assert "wait_for_socket(socket_fd, true" in health_source
    assert "wait_for_socket(socket_fd, true" in v2_source
    assert "getsockopt(socket_fd, SOL_SOCKET, SO_ERROR" in health_source
    assert "getsockopt(socket_fd, SOL_SOCKET, SO_ERROR" in v2_source
