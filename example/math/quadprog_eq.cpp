/****************************************************************************
 *
 * This file is part of the ViSP software.
 * Copyright (C) 2005 - 2017 by Inria. All rights reserved.
 *
 * This software is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file LICENSE.txt at the root directory of this source
 * distribution for additional information about the GNU GPL.
 *
 * For using ViSP with software that can not be combined with the GNU
 * GPL, please contact Inria about acquiring a ViSP Professional
 * Edition License.
 *
 * See http://visp.inria.fr for more information.
 *
 * This software was developed at:
 * Inria Rennes - Bretagne Atlantique
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * France
 *
 * If you have questions regarding the use of this file, please contact
 * Inria at visp@inria.fr
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Description:
 * Example of sequential calls to QP solver with constant equality constraint
 *
 * Authors:
 * Olivier Kermorgant
 *
 *****************************************************************************/
/*!
  \file quadprog_eq.cpp

  \brief Example of sequential calls to QP solver with constant equality constraint
*/

/*!
  \example quadprog_eq.cpp

  Example of sequential calls to QP solver with constant equality constraint
*/

#include <iostream>
#include <visp3/core/vpConfig.h>

#ifdef VISP_HAVE_CPP11_COMPATIBILITY

#include <visp3/core/vpQuadProg.h>
#include <visp3/core/vpTime.h>
#include "qp_plot.h"

int main (int argc, char **argv)
{
  const int n = 20;   // x dim
  const int m = 10;   // equality m < n
  const int p = 30;   // inequality
  const int o = 16;   // cost function
  bool opt_display = true;

  for (int i = 0; i < argc; i++) {
    if (std::string(argv[i]) == "-d")
      opt_display = false;
    else if (std::string(argv[i]) == "-h") {
      std::cout << "\nUsage: " << argv[0] << " [-d] [-h]" << std::endl;
      std::cout << "\nOptions: \n"
                   "  -d \n"
                   "     Disable the image display. This can be useful \n"
                   "     for automatic tests using crontab under Unix or \n"
                   "     using the task manager under Windows.\n"
                   "\n"
                   "  -h\n"
                   "     Print the help.\n"<< std::endl;

      return EXIT_SUCCESS;
    }
  }
  std::srand((long) vpTime::measureTimeMs());

  vpMatrix A, Q, C;
  vpColVector b, d, r;

  A = randM(m,n)*5;
  b = randV(m)*5;
  Q = randM(o,n)*5;
  r = randV(o)*5;
  C = randM(p,n)*5;

  // make sure Cx <= d has a solution within Ax = b
  vpColVector x = A.solveBySVD(b);
  d = C*x;
  for(int i = 0; i < p; ++i)
    d[i] += (5.*rand())/RAND_MAX;

  // solver with stored equality and warm start
  vpQuadProg qp_WS;
  qp_WS.setEqualityConstraint(A, b);

  vpQuadProg qp_ineq_WS;
  qp_ineq_WS.setEqualityConstraint(A, b);

  // timing
  int total = 1000;
  double t, t_WS(0), t_noWS(0), t_ineq_WS(0), t_ineq_noWS(0);
  const double eps = 1e-2;

#ifdef VISP_HAVE_DISPLAY
  QPlot *plot = NULL;
  if (opt_display)
    plot = new QPlot(2, total, {"only equalities", "pre-solving", "equalities + inequalities", "pre-solving / warm start"});
#endif

  for(int k = 0; k < total; ++k)
  {
    // small change on QP data (A and b are constant)
    Q += eps * randM(o,n);
    r += eps * randV(o);
    C += eps * randM(p,n);
    d += eps * randV(p);

    // solve only equalities
    // without warm start
    x = 0;
    t = vpTime::measureTimeMs();
    vpQuadProg::solveQPe(Q, r, A, b, x);

    t_noWS += vpTime::measureTimeMs() - t;
#ifdef VISP_HAVE_DISPLAY
    if (opt_display)
      plot->plot(0, 0, k, t);
#endif

    // with pre-solved Ax = b
    x = 0;
    t = vpTime::measureTimeMs();
    qp_WS.solveQPe(Q, r, x);

    t_WS += vpTime::measureTimeMs() - t;
#ifdef VISP_HAVE_DISPLAY
    if (opt_display)
      plot->plot(0, 1, k, t);
#endif

    // with inequalities
    // without warm start
    x = 0;
    vpQuadProg qp;
    t = vpTime::measureTimeMs();
    qp.solveQP(Q, r, A, b, C, d, x);

    t_ineq_noWS += vpTime::measureTimeMs() - t;
#ifdef VISP_HAVE_DISPLAY
    if (opt_display)
      plot->plot(1, 0, k, t);
#endif

    // with warm start + pre-solving
    x = 0;
    t = vpTime::measureTimeMs();
    qp_ineq_WS.solveQPi(Q, r, C, d, x, true);

    t_ineq_WS += vpTime::measureTimeMs() - t;
#ifdef VISP_HAVE_DISPLAY
    if (opt_display)
      plot->plot(1, 1, k, t);
#endif
  }

  std::cout.precision(3);
  std::cout << "With only equality constraints\n";
  std::cout << "   pre-solving: t = " << t_WS << " ms (for 1 QP = " << t_WS/total << " ms)\n";
  std::cout << "   no pre-solving: t = " << t_noWS << " ms (for 1 QP = " << t_noWS/total << " ms)\n\n";

  std::cout << "With inequality constraints\n";
  std::cout << "   Warm start: t = " << t_ineq_WS << " ms (for 1 QP = " << t_ineq_WS/total << " ms)\n";
  std::cout << "   No warm start: t = " << t_ineq_noWS << " ms (for 1 QP = " << t_ineq_noWS/total << " ms)" << std::endl;

#ifdef VISP_HAVE_DISPLAY
  if (opt_display) {
    plot->wait();
    delete plot;
  }
#endif
}
#else
int main()
{
  std::cout << "You did not build ViSP with C++11 compiler flag" << std::endl;
  std::cout << "Tip:" << std::endl;
  std::cout << "- Configure ViSP again using cmake -DUSE_CPP11=ON, and build again this example" << std::endl;
  return EXIT_SUCCESS;
}
#endif
