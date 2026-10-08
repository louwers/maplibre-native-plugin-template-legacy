#!/usr/bin/env ruby
# Generates PluginGallery.xcodeproj. Requires the xcodeproj gem (gem install xcodeproj).
#
# The app links the plugin products of this repository's Swift package and the
# MapLibre product of the plugin-enabled MapLibre iOS distribution. Set
# MAPLIBRE_IOS_PATH to a local checkout of that package to test unreleased builds;
# use the same value when building so the plugin package resolves it too.
require 'json'
require 'pathname'
require 'xcodeproj'

app_root = Pathname.new(File.expand_path('..', __dir__))
repo_root = app_root.join('../..').cleanpath
Dir.chdir(app_root)

plugins = Dir[repo_root.join('plugins/*/plugin.json').to_s].sort.map do |file|
  config = JSON.parse(File.read(file))
  config.merge('directory' => Pathname.new(file).dirname)
end.select { |plugin| plugin['apple'] }
version = File.read(repo_root.join('maplibre-ios-version.txt')).strip

project = Xcodeproj::Project.new('PluginGallery.xcodeproj')
app = project.new_target(:application, 'PluginGallery', :ios, '15.5')
tests = project.new_target(:ui_test_bundle, 'PluginGalleryUITests', :ios, '15.5')
tests.add_dependency(app)

app_group = project.main_group.new_group('App', 'App')
app.add_file_references(%w[main.m AppDelegate.m PluginCatalog.m].map { |name| app_group.new_file(name) })
%w[AppDelegate.h PluginCatalog.h].each { |name| app_group.new_file(name) }
styles = project.main_group.new_group('Styles')
plugins.each do |plugin|
  style = plugin.dig('demo', 'ios', 'style') or next
  path = plugin['directory'].join("examples/android/assets/#{style}.json").relative_path_from(app_root)
  app.resources_build_phase.add_file_reference(styles.new_file(path.to_s))
end
tests_group = project.main_group.new_group('UITests', 'UITests')
tests.add_file_references([tests_group.new_file('PluginGalleryUITests.swift')])

plugin_package = project.new(Xcodeproj::Project::Object::XCLocalSwiftPackageReference)
plugin_package.relative_path = repo_root.relative_path_from(app_root).to_s
local_maplibre = ENV['MAPLIBRE_IOS_PATH']
if local_maplibre
  maplibre_package = project.new(Xcodeproj::Project::Object::XCLocalSwiftPackageReference)
  maplibre_package.relative_path = local_maplibre
else
  maplibre_package = project.new(Xcodeproj::Project::Object::XCRemoteSwiftPackageReference)
  maplibre_package.repositoryURL = 'https://github.com/louwers/maplibre-ios-with-plugin-api'
  maplibre_package.requirement = { 'kind' => 'exactVersion', 'version' => version }
end
project.root_object.package_references << plugin_package << maplibre_package

products = [['MapLibre', maplibre_package]] + plugins.map { |plugin| [plugin['apple']['product'], plugin_package] }
products.each do |product_name, package|
  product = project.new(Xcodeproj::Project::Object::XCSwiftPackageProductDependency)
  product.package = package if package.is_a?(Xcodeproj::Project::Object::XCRemoteSwiftPackageReference)
  product.product_name = product_name
  app.package_product_dependencies << product
  build_file = project.new(Xcodeproj::Project::Object::PBXBuildFile)
  build_file.product_ref = product
  app.frameworks_build_phase.files << build_file
end
%w[UIKit CoreLocation].each do |name|
  app.frameworks_build_phase.add_file_reference(
    project.frameworks_group.new_file("System/Library/Frameworks/#{name}.framework", :sdk_root))
end
# Use the selected SDK instead of the generator gem's default SDK version.
project.files.select { |file| file.path&.end_with?('Foundation.framework') }.each do |file|
  file.path = 'System/Library/Frameworks/Foundation.framework'
  file.source_tree = 'SDKROOT'
end

[app, tests].each do |target|
  target.build_configurations.each do |config|
    settings = config.build_settings
    settings['GENERATE_INFOPLIST_FILE'] = 'YES'
    settings['CLANG_ENABLE_MODULES'] = 'YES'
    settings['CLANG_ENABLE_OBJC_ARC'] = 'YES'
    settings['SWIFT_VERSION'] = '5.0'
    settings['TARGETED_DEVICE_FAMILY'] = '1,2'
    settings['IPHONEOS_DEPLOYMENT_TARGET'] = '15.5'
    settings['SUPPORTED_PLATFORMS'] = 'iphoneos iphonesimulator'
    settings['SUPPORTS_MACCATALYST'] = 'NO'
    settings['CODE_SIGN_STYLE'] = 'Automatic'
    settings['PRODUCT_BUNDLE_IDENTIFIER'] = "org.maplibre.plugins.#{target.name}"
    settings['LD_RUNPATH_SEARCH_PATHS'] = ['$(inherited)', '@executable_path/Frameworks']
    settings['MARKETING_VERSION'] = '1.0'
    settings['CURRENT_PROJECT_VERSION'] = '1'
  end
end
app.build_configurations.each do |config|
  settings = config.build_settings
  settings['INFOPLIST_KEY_UILaunchScreen_Generation'] = 'YES'
  settings['INFOPLIST_KEY_CFBundleDisplayName'] = 'Plugin Gallery'
  settings['INFOPLIST_KEY_UISupportedInterfaceOrientations'] =
    'UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight'
end
tests.build_configurations.each { |config| config.build_settings['TEST_TARGET_NAME'] = 'PluginGallery' }
project.save

scheme = Xcodeproj::XCScheme.new
scheme.configure_with_targets(app, tests)
scheme.save_as(project.path, 'PluginGallery', true)
