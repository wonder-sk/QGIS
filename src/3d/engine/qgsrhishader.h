/***************************************************************************
  qgsrhishader.h
  --------------------------------------
  Date                 : September 2026
  Copyright            : (C) 2026 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef QGSRHISHADER_H
#define QGSRHISHADER_H

#include <memory>
#include <rhi/qshader.h>

#include "qgis_3d.h"

#include <QString>
#include <QStringList>
#include <QVector>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief One member of a uniform block, as the C++ side expects it to be laid out.
 *
 * Used with QgsRhiShader::checkUniformBlockLayout() to verify a material's uniform struct
 * against the shader it feeds.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
struct QgsUniformMember
{
    //! Name of the member in the GLSL uniform block
    QString name;
    //! Byte offset of the member within the block, normally from offsetof()
    int offset = 0;
    //! Size of the member in bytes, normally from sizeof()
    int size = 0;
};

/**
 * \ingroup qgis_3d
 * \brief A vertex and fragment shader pair, baked to all target graphics APIs at build time.
 *
 * Shaders are written once as Vulkan-style GLSL and translated by qsb during the build into
 * SPIR-V, GLSL, HLSL and MSL, all packed into a single .qsb file. Nothing is compiled from
 * source at runtime, so there is no per-run translation cost and no shader cache to maintain.
 *
 * The baked file also carries reflection data, which is what lets vertex attributes stay
 * name-based in QGIS code: inputLocation() resolves a name to the location QRhi needs.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsRhiShader
{
  public:
    /**
     * Returns the shader built from the given baked vertex and fragment stages.
     *
     * The paths are usually Qt resource paths such as ":/shaders/rhi/phong.vert.qsb".
     * Shaders are cached by path, so asking for the same pair twice is cheap and returns
     * the same object. Returns NULLPTR if either stage could not be loaded.
     */
    static std::shared_ptr<QgsRhiShader> load( const QString &vertexPath, const QString &fragmentPath );

    //! Drops every cached shader. Called when the 3D subsystem shuts down.
    static void clearCache();

    //! Returns the baked vertex stage
    const QShader &vertexShader() const { return mVertexShader; }

    //! Returns the baked fragment stage
    const QShader &fragmentShader() const { return mFragmentShader; }

    //! Returns the resource path the vertex stage was loaded from
    QString vertexPath() const { return mVertexPath; }

    //! Returns the resource path the fragment stage was loaded from
    QString fragmentPath() const { return mFragmentPath; }

    /**
     * Returns the vertex input location of the attribute called \a name, or -1 if the
     * vertex shader has no such input.
     *
     * An attribute the shader does not read is not an error: the same geometry is drawn
     * by several shaders, and a depth-only pass reads fewer attributes than a shading pass.
     */
    int inputLocation( const QString &name ) const;

    /**
     * Returns the names of every vertex input the vertex stage declares.
     *
     * A geometry has to supply an attribute for each of them. A shader input that no
     * attribute matches is not merely unused: the graphics pipeline fails to build, and
     * nothing is drawn at all. Which inputs a shader declares is therefore part of what
     * picks one baked variant over another.
     */
    QStringList inputNames() const;

    //! Returns the combined image samplers the fragment stage declares, in binding order
    QVector<QShaderDescription::InOutVariable> samplers() const;

    //! Returns the size in bytes of the uniform block at \a binding, or -1 if there is none
    int uniformBlockSize( int binding ) const;

    /**
     * Checks that every input the fragment stage reads is written by the vertex stage,
     * under the same name and at the same location.
     *
     * Matching locations is enough for SPIR-V, and so for the Vulkan, Metal and Direct3D
     * backends, but the OpenGL backend links varyings by name. A pair of stages that
     * disagree about a name therefore draws correctly everywhere except on OpenGL, where
     * the program fails to link and nothing is drawn at all. load() runs this check in
     * debug builds so that the mismatch is reported wherever the shader is used, rather
     * than only on the one backend that minds.
     *
     * Returns TRUE if the two stages agree, and describes the first mismatch in \a error
     * otherwise.
     */
    bool checkStageInterface( QString *error = nullptr ) const;

    /**
     * Checks that the uniform block at \a binding is laid out the way the C++ side assumes.
     *
     * A mismatch between a material's uniform struct and its GLSL block corrupts every
     * value after the first wrong offset, and does so silently, so materials verify their
     * layout once at load time. Returns TRUE if every member in \a expected is present at
     * the stated offset and size, and describes the first mismatch in \a error otherwise.
     */
    bool checkUniformBlockLayout( int binding, const QVector<QgsUniformMember> &expected, QString *error = nullptr ) const;

  private:
    QgsRhiShader() = default;

    static QShader loadStage( const QString &path );

    QShader mVertexShader;
    QShader mFragmentShader;
    QString mVertexPath;
    QString mFragmentPath;
};

#endif // QGSRHISHADER_H
