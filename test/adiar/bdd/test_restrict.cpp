go_bandit([]() {
  describe("adiar/bdd/restrict.cpp", []() {
    /*
             1         ---- x0
            / \
            | 2        ---- x1
            |/ \
            3   4      ---- x2
           / \ / \
           F T T 5     ---- x3
                / \
                F T
    */

    ptr_t sink_T = create_sink_ptr(true);
    ptr_t sink_F = create_sink_ptr(false);

    node_t n5 = create_node(3,0, sink_F, sink_T);
    node_t n4 = create_node(2,1, sink_T, n5.uid);
    node_t n3 = create_node(2,0, sink_F, sink_T);
    node_t n2 = create_node(1,0, n3.uid, n4.uid);
    node_t n1 = create_node(0,0, n3.uid, n2.uid);

    node_file bdd;

    { // Garbage collect writer to free write-lock
      node_writer bdd_w(bdd);
      bdd_w << n5 << n4 << n3 << n2 << n1;
    }

    it("should bridge level [1] Assignment: (_,_,T,_)", [&]() {
      /*
                 1      ---- x0
                / \
                T 2     ---- x1
                 / \
                 T |
                   |
                   5    ---- x3
                  / \
                  F T
      */

      assignment_file assignment;

      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(2, true);
      }

      __bdd output = bdd_restrict(bdd, assignment);

      node_arc_test_stream node_arcs(output);

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n1.uid), n2.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n2.uid), n5.uid }));

      AssertThat(node_arcs.can_pull(), Is().False());

      sink_arc_test_stream sink_arcs(output);

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n1.uid, sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n2.uid, sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n5.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n5.uid), sink_T }));

      level_info_test_stream<arc_t> meta_arcs(output);

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(0,1u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(1,1u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(3,1u)));

      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<arc_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(1u));

      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(1u));
      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(3u));
    });

    it("should bridge levels [2]. Assignment: (_,F,_,_)", [&]() {
      /*
                 1      ---- x0
                / \
                | |
                \ /
                 3      ---- x2
                / \
                F T
      */

      assignment_file assignment;

      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(1, false);
      }

      __bdd output = bdd_restrict(bdd, assignment);

      node_arc_test_stream node_arcs(output);

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { n1.uid, n3.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n1.uid), n3.uid }));

      AssertThat(node_arcs.can_pull(), Is().False());

      sink_arc_test_stream sink_arcs(output);

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n3.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n3.uid), sink_T }));

      level_info_test_stream<arc_t> meta_arcs(output);

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(0,1u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(2,1u)));

      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<arc_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(2u));

      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(1u));
      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(1u));
    });

    it("should bridge levels [3]. Assignment: (_,T,_,_)", [&]() {
      /*
                  1         ---- x0
                 / \
                /   \
                |   |
                3   4       ---- x2
               / \ / \
               F T T 5      ---- x3
                    / \
                    F T
      */

      assignment_file assignment;

      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(1, true);
      }

      __bdd output = bdd_restrict(bdd, assignment);

      node_arc_test_stream node_arcs(output);

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { n1.uid, n3.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n1.uid), n4.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n4.uid), n5.uid }));

      AssertThat(node_arcs.can_pull(), Is().False());

      sink_arc_test_stream sink_arcs(output);

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n3.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n3.uid), sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n4.uid, sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n5.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n5.uid), sink_T }));

      level_info_test_stream<arc_t> meta_arcs(output);

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(0,1u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(2,2u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(3,1u)));

      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<arc_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(2u));

      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(2u));
      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(3u));
    });

    it("should remove root. Assignment: (T,_,_,F)", [&]() {
      /*
                  2     ---- x1
                 / \
                /   \
                3   4   ---- x2
               / \ / \
               F T T F
      */

      assignment_file assignment;

      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(0, true)
           << create_assignment(3, false);
      }

      __bdd output = bdd_restrict(bdd, assignment);

      node_arc_test_stream node_arcs(output);

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { n2.uid, n3.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n2.uid), n4.uid }));

      AssertThat(node_arcs.can_pull(), Is().False());

      sink_arc_test_stream sink_arcs(output);

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n3.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n3.uid), sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n4.uid, sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n4.uid), sink_F }));

      level_info_test_stream<arc_t> meta_arcs(output);

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(1,1u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(2,2u)));

      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<arc_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(2u));

      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(2u));
      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(2u));
    });

    it("should ignore skipped variables. Assignment: (F,T,_,F)", [&]() {
      /*
                 3      ---- x2
                / \
                F T
      */

      assignment_file assignment;

      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(0, false)
           << create_assignment(1, true)
           << create_assignment(3, false);
      }

      __bdd output = bdd_restrict(bdd, assignment);

      node_arc_test_stream node_arcs(output);

      AssertThat(node_arcs.can_pull(), Is().False());

      sink_arc_test_stream sink_arcs(output);

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n3.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n3.uid), sink_T }));

      level_info_test_stream<arc_t> meta_arcs(output);

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(2,1u)));

      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<arc_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(0u));

      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(1u));
      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(1u));
    });

    it("should return F sink. Assignment: (F,_,F,_)", [&]() {
      assignment_file assignment;

      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(0, false)
           << create_assignment(2, false);
      }

      __bdd output = bdd_restrict(bdd, assignment);

      node_test_stream out_nodes(output);

      AssertThat(out_nodes.can_pull(), Is().True());
      AssertThat(out_nodes.pull(), Is().EqualTo(create_sink(false)));
      AssertThat(out_nodes.can_pull(), Is().False());

      level_info_test_stream<node_t> meta_arcs(output);
      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<node_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(0u));

      AssertThat(output.get<node_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(1u));
      AssertThat(output.get<node_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(0u));
    });

    it("should return T sink. Assignment: (T,T,F,_)", [&]() {
      assignment_file assignment;

      {  // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(0, true)
           << create_assignment(1, true)
           << create_assignment(2, false);
      }

      __bdd output = bdd_restrict(bdd, assignment);

      node_test_stream out_nodes(output);

      AssertThat(out_nodes.can_pull(), Is().True());
      AssertThat(out_nodes.pull(), Is().EqualTo(create_sink(true)));
      AssertThat(out_nodes.can_pull(), Is().False());

      level_info_test_stream<node_t> meta_arcs(output);
      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<node_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(0u));

      AssertThat(output.get<node_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(0u));
      AssertThat(output.get<node_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(1u));
    });

    it("should return input unchanged when given a T sink", [&]() {
      node_file T_file;

      { // Garbage collect writer to free write-lock
        node_writer Tw(T_file);
        Tw << create_sink(true);
      }

      assignment_file assignment;

      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(0, true)
           << create_assignment(2, true)
           << create_assignment(42, false);
      }

      __bdd out = bdd_restrict(T_file, assignment);

      AssertThat(out.get<node_file>()._file_ptr, Is().EqualTo(T_file._file_ptr));
      AssertThat(out.negate, Is().False());
    });

    it("should return input unchanged when given a F sink", [&]() {
      node_file F_file;

      { // Garbage collect writer to free write-lock
        node_writer Fw(F_file);
        Fw << create_sink(false);
      }

      assignment_file assignment;

      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(2, true)
           << create_assignment(21, true)
           << create_assignment(28, false);
      }

      __bdd out = bdd_restrict(F_file, assignment);

      AssertThat(out.get<node_file>()._file_ptr, Is().EqualTo(F_file._file_ptr));
      AssertThat(out.negate, Is().False());
    });

    it("should return input unchanged when given an empty assignment", [&]() {
      assignment_file assignment;

      __bdd out = bdd_restrict(bdd, assignment);

      AssertThat(out.get<node_file>()._file_ptr, Is().EqualTo(bdd._file_ptr));
      AssertThat(out.negate, Is().False());
    });

    it("should return input unchanged when assignment that is disjoint of its live variables", [&]() {
      assignment_file assignment;
      { // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(5, false)
           << create_assignment(6, true)
           << create_assignment(7, true)
          ;
      }

      __bdd out = bdd_restrict(bdd, assignment);

      AssertThat(out.get<node_file>()._file_ptr, Is().EqualTo(bdd._file_ptr));
      AssertThat(out.negate, Is().False());
    });

    it("should have sink arcs restricted to a sink sorted [1]", []() {
      /*
                    1                 1         ---- x0
                   / \              /   \
                  2   3     =>     2     3      ---- x1
                 / \ / \         /   \  / \
                 4 F T F         F*  F  T F     ---- x2
                / \
                T F              * This arc will be resolved as the last one
      */
      ptr_t sink_T = create_sink_ptr(true);
      ptr_t sink_F = create_sink_ptr(false);

      node_file node_input;

      node_t n4 = create_node(2,0, sink_T, sink_F);
      node_t n3 = create_node(1,1, sink_T, sink_F);
      node_t n2 = create_node(1,0, n4.uid, sink_F);
      node_t n1 = create_node(0,0, n2.uid, n3.uid);

      { // Garbage collect writer to free write-lock
        node_writer inw(node_input);
        inw << n4 << n3 << n2 << n1;
      }

      assignment_file assignment;

      {  // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(2, true);
      }

      __bdd output = bdd_restrict(node_input, assignment);

      node_arc_test_stream node_arcs(output);

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { n1.uid, n2.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n1.uid), n3.uid }));

      AssertThat(node_arcs.can_pull(), Is().False());

      sink_arc_test_stream sink_arcs(output);

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n2.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n2.uid), sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n3.uid, sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n3.uid), sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().False());

      level_info_test_stream<arc_t> meta_arcs(output);

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(0,1u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(1,2u)));

      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<arc_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(2u));

      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(3u));
      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(1u));
    });

    it("should have sink arcs restricted to a sink sorted [2]", []() {
      /*
                    1                _ 1 _
                   / \              /     \
                  2   3     =>     2       3
                 / \ / \         /   \   /   \
                 4 F 5 F         F*  F   T*  F
                / \ / \
                T F F T          * Both these will be resolved out-of-order!
      */
      ptr_t sink_T = create_sink_ptr(true);
      ptr_t sink_F = create_sink_ptr(false);

      node_file node_input;

      node_t n5 = create_node(2,1, sink_F, sink_T);
      node_t n4 = create_node(2,0, sink_T, sink_F);
      node_t n3 = create_node(1,1, n5.uid, sink_F);
      node_t n2 = create_node(1,0, n4.uid, sink_F);
      node_t n1 = create_node(0,0, n2.uid, n3.uid);

      { // Garbage collect writer to free write-lock
        node_writer inw(node_input);
        inw << n5 << n4 << n3 << n2 << n1;
      }

      assignment_file assignment;

      {  // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(2, true);
      }

      __bdd output = bdd_restrict(node_input, assignment);

      node_arc_test_stream node_arcs(output);

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { n1.uid, n2.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n1.uid), n3.uid }));

      AssertThat(node_arcs.can_pull(), Is().False());

      sink_arc_test_stream sink_arcs(output);

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n2.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n2.uid), sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n3.uid, sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n3.uid), sink_F} ));

      AssertThat(sink_arcs.can_pull(), Is().False());

      level_info_test_stream<arc_t> meta_arcs(output);

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(0,1u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(1,2u)));

      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<arc_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(2u));

      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(3u));
      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(1u));
    });

    it("should skip 'dead' nodes", [&]() {
      /*
                        1           ---- x0
                      /   \
                     2     3        ---- x1
                    / \   / \
                   4  5   6  7      ---- x2
                  / \/ \ / \/ \
                  T F  8 F  9 T     ---- x3
                      / \  / \
                      F T  T F

                 Here, node 4 and 6 are going to be dead, when x1 -> T.
      */

      node_file dead_bdd;

      node_t n9 = create_node(3,1, sink_T, sink_F);
      node_t n8 = create_node(3,0, sink_F, sink_T);
      node_t n7 = create_node(2,3, n9.uid, sink_T);
      node_t n6 = create_node(2,2, sink_T, n9.uid);
      node_t n5 = create_node(2,1, sink_F, n8.uid);
      node_t n4 = create_node(2,0, sink_T, sink_F);
      node_t n3 = create_node(1,1, n6.uid, n7.uid);
      node_t n2 = create_node(1,0, n4.uid, n5.uid);
      node_t n1 = create_node(0,0, n2.uid, n3.uid);

      { // Garbage collect writer to free write-lock
        node_writer dead_w(dead_bdd);
        dead_w << n9 << n8 << n7 << n6 << n5 << n4 << n3 << n2 << n1;
      }

      assignment_file assignment;

      {  // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(1, true);
      }

      __bdd output = bdd_restrict(dead_bdd, assignment);

      node_arc_test_stream node_arcs(output);

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { n1.uid, n5.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n1.uid), n7.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { flag(n5.uid), n8.uid }));

      AssertThat(node_arcs.can_pull(), Is().True());
      AssertThat(node_arcs.pull(), Is().EqualTo(arc { n7.uid, n9.uid }));

      AssertThat(node_arcs.can_pull(), Is().False());

      sink_arc_test_stream sink_arcs(output);

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n5.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n7.uid), sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n8.uid, sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n8.uid), sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { n9.uid, sink_T }));

      AssertThat(sink_arcs.can_pull(), Is().True());
      AssertThat(sink_arcs.pull(), Is().EqualTo(arc { flag(n9.uid), sink_F }));

      AssertThat(sink_arcs.can_pull(), Is().False());

      level_info_test_stream<arc_t> meta_arcs(output);

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(0,1u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(2,2u)));

      AssertThat(meta_arcs.can_pull(), Is().True());
      AssertThat(meta_arcs.pull(), Is().EqualTo(create_level_info(3,2u)));

      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(output.get<arc_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(2u));

      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(3u));
      AssertThat(output.get<arc_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(3u));
    });

    it("should return sink-child of restricted root [assignment = T]", [&]() {
      node_file sink_child_of_root_bdd;

      node_t n2 = create_node(2,MAX_ID, sink_T, sink_T);
      node_t n1 = create_node(1,MAX_ID, n2.uid, sink_F);

      { // Garbage collect writer to free write-lock
        node_writer dead_w(sink_child_of_root_bdd);
        dead_w << n2 << n1;
      }

      assignment_file assignment;

      {  // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(1, true);
      }

      __bdd out = bdd_restrict(sink_child_of_root_bdd, assignment);

      node_test_stream out_nodes(out);

      AssertThat(out_nodes.can_pull(), Is().True());
      AssertThat(out_nodes.pull(), Is().EqualTo(create_sink(false)));
      AssertThat(out_nodes.can_pull(), Is().False());

      level_info_test_stream<node_t> meta_arcs(out);
      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(out.get<node_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(0u));

      AssertThat(out.get<node_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(1u));
      AssertThat(out.get<node_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(0u));
    });

    it("should return sink-child of restricted root [assignment = F]", [&]() {
      node_file sink_child_of_root_bdd;

      node_t n2 = create_node(2,MAX_ID, sink_T, sink_T);
      node_t n1 = create_node(0,MAX_ID, sink_T, n2.uid);

      { // Garbage collect writer to free write-lock
        node_writer dead_w(sink_child_of_root_bdd);
        dead_w << n2 << n1;
      }

      assignment_file assignment;

      {  // Garbage collect writer to free write-lock
        assignment_writer aw(assignment);
        aw << create_assignment(0, false);
      }

      __bdd out = bdd_restrict(sink_child_of_root_bdd, assignment);

      node_test_stream out_nodes(out);

      AssertThat(out_nodes.can_pull(), Is().True());
      AssertThat(out_nodes.pull(), Is().EqualTo(create_sink(true)));
      AssertThat(out_nodes.can_pull(), Is().False());

      level_info_test_stream<node_t> meta_arcs(out);
      AssertThat(meta_arcs.can_pull(), Is().False());

      AssertThat(out.get<node_file>()._file_ptr->max_1level_cut, Is().GreaterThanOrEqualTo(0u));

      AssertThat(out.get<node_file>()._file_ptr->number_of_sinks[0], Is().EqualTo(0u));
      AssertThat(out.get<node_file>()._file_ptr->number_of_sinks[1], Is().EqualTo(1u));
    });
  });
 });
