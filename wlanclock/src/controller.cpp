#include <ArduinoJson.h>

#include "controller.h"
#include "httpServer.h"
#include "gui.h"
#include "wifi.h"

void Controller::index() {
  String content = Gui::index();

  HttpServer::web.sendHeader("Location", "http://" + HttpServer::ip);
  HttpServer::web.send(200, "text/html", content);
}

void Controller::saveTime() {
/*  String json = HttpServer::web.arg("plain");

  DynamicJsonDocument doc(2048);
  deserializeJson(doc, json);

  Config::automatic_timezone = doc["tz_auto"].as<int>() == 1;

  // store the last known value anyway.
  Config::timezone = doc["tz"].as<int>();

  if (Config::automatic_timezone) {
    UtcOffset::updateLocalizedUtcOffset();
  }

  Config::ntp = doc["ntp"].as<String>();

  Config::save();
  Grid::setTime(Time::hour, Time::minute);
*/
  HttpServer::web.send(200, "text/html", "");
}

void Controller::deleteWiFi() {
  Wifi::reset();
  ESP.restart();
  HttpServer::web.send(200, "text/html", "");
}

void Controller::getLogoSvg() {
  String content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg width=\"1024\" height=\"1024\" version=\"1.1\" viewBox=\"0 0 270.93 270.93\" xmlns=\"http://www.w3.org/2000/svg\"><rect x=\".13216\" y=\".13216\" width=\"270.67\" height=\"270.67\" rx=\"26.433\" ry=\"26.433\" fill=\"#333\"/><rect x=\"15.992\" y=\"15.992\" width=\"238.95\" height=\"238.95\" rx=\"23.335\" ry=\"23.335\" fill=\"#eee\"/><g fill=\"#3d72a8\"><circle cx=\"90.488\" cy=\"50.271\" r=\"13.098\"/><circle cx=\"47.096\" cy=\"50.271\" r=\"13.098\"/><circle cx=\"90.488\" cy=\"93.662\" r=\"13.098\"/><circle cx=\"133.88\" cy=\"93.662\" r=\"13.098\"/><circle cx=\"177.4\" cy=\"93.662\" r=\"13.098\"/><circle cx=\"133.88\" cy=\"180.45\" r=\"13.098\"/><circle cx=\"177.4\" cy=\"180.45\" r=\"13.098\"/><circle cx=\"220.79\" cy=\"180.45\" r=\"13.098\"/><circle cx=\"47.096\" cy=\"180.45\" r=\"13.098\"/></g></svg>";

  HttpServer::web.send(200, "image/svg+xml", content);
}

void Controller::notFound() {
  if(HttpServer::web.method() == HTTP_OPTIONS) {
    HttpServer::web.sendHeader("Allow", "DELETE,GET,HEAD,OPTIONS,POST,PUT");
    HttpServer::web.send(200, "text/html", "");

    return;
  }

  HttpServer::web.send(404, "text/html", "NOT FOUND");
}
