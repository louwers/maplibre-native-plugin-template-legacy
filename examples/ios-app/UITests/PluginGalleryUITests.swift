import XCTest

/// Launches every plugin scene and attaches a screenshot of the rendered map.
final class PluginGalleryUITests: XCTestCase {
    func testScenesLaunch() throws {
        for scene in ["ngon", "rectangle"] {
            let app = XCUIApplication()
            app.launchEnvironment["PLUGIN_SCENE"] = scene
            app.launch()
            // A failed plugin registration makes the app exit before showing the scene picker.
            XCTAssertTrue(app.segmentedControls.firstMatch.waitForExistence(timeout: 20), "\(scene): gallery did not start")
            sleep(8) // Allow tiles to load and plugin layers to render.
            let attachment = XCTAttachment(screenshot: app.screenshot())
            attachment.name = scene
            attachment.lifetime = .keepAlways
            add(attachment)
            app.terminate()
        }
    }
}
