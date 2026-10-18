// SPDX-License-Identifier: MIT
#include <QApplication>
#include <KSvg/FrameSvg>
#include <QImage>
#include <QFileInfo>
#include <QTextStream>
int main(int argc,char **argv){QApplication app(argc,argv);for(auto path:{QString::fromUtf8(STUDIO_PANEL_SVG),QString::fromUtf8(STUDIO_PANEL_SVG).replace("/widgets/","/opaque/widgets/")})for(const auto color:{QColor("#112018"),QColor("#f5f0fb"),QColor("#221d36")}){KSvg::FrameSvg svg;svg.setImagePath(QFileInfo(path).absoluteFilePath());svg.setColor(KSvg::Svg::Background,color);svg.resizeFrame(QSizeF(42,800));auto image=svg.framePixmap().toImage();if(image.size()!=QSize(42,800))return 2;for(const auto point:{QPoint(0,0),QPoint(20,400),QPoint(41,799)}){auto actual=image.pixelColor(point);if(actual!=color){QTextStream(stderr)<<"Expected "<<color.name()<<" got "<<actual.name(QColor::HexArgb)<<"\n";return 1;}}}QTextStream(stdout)<<"PASS: native KSvg panel corners and centre exactly match three supplied frame colours.\n";}
