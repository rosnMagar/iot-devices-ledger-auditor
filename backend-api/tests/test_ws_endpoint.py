# IOT-61 — the browser-facing endpoint. Fanout and back-pressure are covered in
# test_hub.py; what matters here is that connecting subscribes and, more
# importantly, that disconnecting unsubscribes.

from fastapi.testclient import TestClient

from app.main import app, hub


def test_connecting_subscribes_and_disconnecting_cleans_up() -> None:
    # A subscription leaked per closed browser would grow unbounded and keep
    # queueing frames nobody reads.
    before = hub.subscriber_count
    client = TestClient(app)

    with client.websocket_connect("/ws/blocks"):
        assert hub.subscriber_count == before + 1

    assert hub.subscriber_count == before


def test_several_browsers_subscribe_independently() -> None:
    client = TestClient(app)
    before = hub.subscriber_count

    with client.websocket_connect("/ws/blocks"):
        with client.websocket_connect("/ws/blocks"):
            assert hub.subscriber_count == before + 2
        assert hub.subscriber_count == before + 1

    assert hub.subscriber_count == before


def test_repeated_connections_do_not_leak() -> None:
    client = TestClient(app)
    before = hub.subscriber_count

    for _ in range(5):
        with client.websocket_connect("/ws/blocks"):
            pass

    assert hub.subscriber_count == before


def test_feed_status_reports_browser_subscribers() -> None:
    client = TestClient(app)
    with client.websocket_connect("/ws/blocks"):
        body = client.get("/feed/status").json()
        assert body["browser_subscribers"] >= 1
        assert "frames_broadcast" in body
