#import "AppDelegate.h"

#import <MapLibre/MapLibre.h>

#import "PluginCatalog.h"

@interface AppDelegate () <MLNMapViewDelegate>
@property(nonatomic, strong) MLNMapView *mapView;
@property(nonatomic, strong) UISegmentedControl *scenes;
@property(nonatomic, copy) NSArray<NSDictionary *> *sceneDefinitions;
@end

@implementation AppDelegate

- (NSDictionary *)selectedScene {
    return self.sceneDefinitions[(NSUInteger)self.scenes.selectedSegmentIndex];
}

- (NSURL *)selectedStyleURL {
    NSURL *url = [[NSBundle mainBundle] URLForResource:self.selectedScene[@"style"] withExtension:@"json"];
    NSAssert(url, @"The bundled plugin style is missing");
    return url;
}

- (void)mapView:(MLNMapView *)mapView didFinishLoadingStyle:(MLNStyle *)style {
    NSArray<NSNumber *> *camera = self.selectedScene[@"camera"];
    CLLocationCoordinate2D center = CLLocationCoordinate2DMake(camera[0].doubleValue, camera[1].doubleValue);
    mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:center
                                                          altitude:camera[2].doubleValue
                                                             pitch:camera[3].doubleValue
                                                           heading:camera[4].doubleValue];
}

- (void)mapViewDidFailLoadingMap:(MLNMapView *)mapView withError:(NSError *)error {
    NSLog(@"Map failed: %@", error.localizedDescription);
}

- (void)selectScene {
    self.mapView.styleURL = [self selectedStyleURL];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Plugins must be registered before the first style that uses their layer types is loaded.
    NSError *error = nil;
    if (!RegisterGalleryPlugins(&error)) {
        NSLog(@"Unable to register plugin: %@", error);
        return NO;
    }

    self.sceneDefinitions = GalleryScenes();
    NSMutableArray<NSString *> *titles = [NSMutableArray array];
    NSInteger selected = 0;
    NSString *requested = NSProcessInfo.processInfo.environment[@"PLUGIN_SCENE"];
    for (NSDictionary *scene in self.sceneDefinitions) {
        if ([scene[@"key"] isEqualToString:requested]) selected = (NSInteger)titles.count;
        [titles addObject:scene[@"title"]];
    }

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController *controller = [[UIViewController alloc] init];
    self.scenes = [[UISegmentedControl alloc] initWithItems:titles];
    self.scenes.selectedSegmentIndex = selected;
    [self.scenes addTarget:self action:@selector(selectScene) forControlEvents:UIControlEventValueChanged];

    self.mapView = [[MLNMapView alloc] initWithFrame:controller.view.bounds styleURL:[self selectedStyleURL]];
    self.mapView.delegate = self;
    self.mapView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [controller.view addSubview:self.mapView];

    self.scenes.translatesAutoresizingMaskIntoConstraints = NO;
    self.scenes.backgroundColor = UIColor.systemBackgroundColor;
    [controller.view addSubview:self.scenes];
    [NSLayoutConstraint activateConstraints:@[
        [self.scenes.topAnchor constraintEqualToAnchor:controller.view.safeAreaLayoutGuide.topAnchor constant:12],
        [self.scenes.centerXAnchor constraintEqualToAnchor:controller.view.centerXAnchor],
    ]];
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];
    return YES;
}

@end
