// Copyright (c) 2017-2022, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// [CLI11:public_includes:set]
#include <algorithm>
#include <string>
#include <vector>
// [CLI11:public_includes:end]

#include "App.hpp"
#include "FormatterFwd.hpp"

namespace CLI {
// [CLI11:leap_formatter_hpp:verbatim]

class LeapFormatter : public FormatterBase {
   const char* tree_line = u8"\u2502";
   const char* tree_angle = u8"\u2514";
   const char* tree_fork = u8"\u251C";

public:
   LeapFormatter() = default;
   LeapFormatter(const LeapFormatter&) = default;
   LeapFormatter(LeapFormatter&&) = default;

   /// @name Overridables
   ///@{

   /// This prints out a group of options with title
   ///
   virtual std::string make_group(std::string group, bool is_positional, std::vector<const Option*> opts) const {
      std::stringstream out;

      out << "\n"
          << group << ":\n";
      for(const Option* opt: opts) {
         out << make_option(opt, is_positional);
      }

      return out.str();
   }

   /// This prints out just the positionals "group"
   virtual std::string make_positionals(const App* app) const {
      std::vector<const Option*> opts =
            app->get_options([](const Option* opt) { return !opt->get_group().empty() && opt->get_positional(); });

      if(opts.empty())
         return std::string();

      return make_group(get_label("Positionals"), true, opts);
   }

   /// This prints out all the groups of options
   std::string make_groups(const App* app, AppFormatMode mode) const {
      std::stringstream out;
      std::vector<std::string> groups = app->get_groups();

      // Options
      for(const std::string& group: groups) {
         std::vector<const Option*> opts = app->get_options([app, mode, &group](const Option* opt) {
            return opt->get_group() == group                       // Must be in the right group
                   && opt->nonpositional()                         // Must not be a positional
                   && (mode != AppFormatMode::Sub                  // If mode is Sub, then
                       || (app->get_help_ptr() != opt              // Ignore help pointer
                           && app->get_help_all_ptr() != opt       // Ignore help all pointer
                           && app->get_autocomplete_ptr() != opt));// Ignore auto-complete pointer
         });
         if(!group.empty() && !opts.empty()) {
            out << make_group(group, false, opts);

            if(group != groups.back())
               out << "\n";
         }
      }

      return out.str();
   }

   /// This prints out all the subcommands
   virtual std::string make_subcommands(const App* app, AppFormatMode mode) const {
      std::stringstream out;

      std::vector<const App*> subcommands = app->get_subcommands({});

      // Make a list in definition order of the groups seen
      std::vector<std::string> subcmd_groups_seen;
      for(const App* com: subcommands) {
         if(com->get_name().empty()) {
            if(!com->get_group().empty()) {
               out << make_expanded(com);
            }
            continue;
         }
         std::string group_key = com->get_group();
         if(!group_key.empty() &&
            std::find_if(subcmd_groups_seen.begin(), subcmd_groups_seen.end(), [&group_key](std::string a) {
               return detail::to_lower(a) == detail::to_lower(group_key);
            }) == subcmd_groups_seen.end())
            subcmd_groups_seen.push_back(group_key);
      }

      // For each group, filter out and print subcommands
      for(const std::string& group: subcmd_groups_seen) {
         if(mode != AppFormatMode::SubCompact) {// do not show "Subcommands" header for nested tems in compact mode
            out << "\n"
                << group << ":\n";
         }
         std::vector<const App*> subcommands_group = app->get_subcommands([&group](const App* sub_app) {
            return detail::to_lower(sub_app->get_group()) == detail::to_lower(group);
         });
         for(const App* new_com: subcommands_group) {
            if(new_com->get_name().empty())
               continue;

            std::string tree_symbol = (subcommands_group.back() == new_com ? tree_angle : tree_fork);
            std::string line_symbol = (subcommands_group.back() == new_com ? "" : tree_line);
            std::string subc_symbol = "";

            const App* parent = app->get_parent();
            if(parent != nullptr) {
               std::vector<const App*> sc_group = parent->get_subcommands([&group](const App* sub_app) {
                  return detail::to_lower(sub_app->get_group()) == detail::to_lower(group);
               });
               if(sc_group.back() != app) {
                  subc_symbol = tree_line;
               }
            }

            switch(mode) {
               case AppFormatMode::All:
                  out << tree_symbol << new_com->help(new_com->get_name(), AppFormatMode::Sub);
                  out << "\n";
                  break;
               case AppFormatMode::AllCompact:

                  out << tree_symbol << new_com->help(new_com->get_name(), AppFormatMode::SubCompact);
                  out << line_symbol;
                  out << "\n";
                  break;
               case AppFormatMode::Normal:
               case AppFormatMode::Sub:
                  out << make_subcommand(new_com);
                  break;
               case AppFormatMode::SubCompact:
                  out << tree_symbol << make_expanded(new_com, mode);
                  break;
            }
         }
      }

      return out.str();
   }

   /// This prints out a subcommand
   virtual std::string make_subcommand(const App* sub) const {
      std::stringstream out;
      detail::format_help(out, sub->get_display_name(true), sub->get_description(), column_width_);
      return out.str();
   }

   /// This prints out a subcommand in help-all
   virtual std::string make_expanded(const App* sub, AppFormatMode mode = AppFormatMode::Sub) const {
      std::stringstream out;
      std::string tmp;
      std::string subc_symbol = " ";

      if(mode == AppFormatMode::SubCompact) {

         detail::format_help(out, sub->get_display_name(true), sub->get_description(), column_width_);

         out << make_subcommands(sub, mode);
      } else {
         out << sub->get_display_name(true) << "\n";

         out << make_description(sub);
         if(sub->get_name().empty() && !sub->get_aliases().empty()) {
            detail::format_aliases(out, sub->get_aliases(), column_width_ + 2);
         }
         out << make_positionals(sub);
         out << make_groups(sub, mode);
         out << make_subcommands(sub, mode);
      }

      // Drop blank spaces
      tmp = detail::find_and_replace(out.str(), "\n\n", "\n");
      tmp = tmp.substr(0, tmp.size() - 1);// Remove the final '\n'

      //
      auto group = sub->get_parent()->get_group();
      std::vector<const App*> sc_group = sub->get_parent()->get_subcommands(
            [&group](const App* sub_app) { return detail::to_lower(sub_app->get_group()) == detail::to_lower(group); });

      if(sc_group.back() != sub) {
         subc_symbol = tree_line;
      }

      // Indent all but the first line (the name)
      return detail::find_and_replace(tmp, "\n", "\n" + subc_symbol + "  ") + "\n";
   }

   /// This prints out all the groups of options
   virtual std::string make_footer(const App* app) const {
      std::string footer = app->get_footer();
      if(footer.empty()) {
         return std::string{};
      }
      return footer + "\n";
   }

   /// This displays the description line
   virtual std::string make_description(const App* app) const {
      std::string desc = app->get_description();
      auto min_options = app->get_require_option_min();
      auto max_options = app->get_require_option_max();
      if(app->get_required()) {
         desc += " REQUIRED ";
      }
      if((max_options == min_options) && (min_options > 0)) {
         if(min_options == 1) {
            desc += " \n[Exactly 1 of the following options is required]";
         } else {
            desc += " \n[Exactly " + std::to_string(min_options) + "options from the following list are required]";
         }
      } else if(max_options > 0) {
         if(min_options > 0) {
            desc += " \n[Between " + std::to_string(min_options) + " and " + std::to_string(max_options) +
                    " of the follow options are required]";
         } else {
            desc += " \n[At most " + std::to_string(max_options) + " of the following options are allowed]";
         }
      } else if(min_options > 0) {
         desc += " \n[At least " + std::to_string(min_options) + " of the following options are required]";
      }
      return (!desc.empty()) ? desc + "\n" : std::string{};
   }

   /// This displays the usage line
   virtual std::string make_usage(const App* app, std::string name) const {
      std::stringstream out;

      out << get_label("Usage") << ":" << (name.empty() ? "" : " ") << name;

      std::vector<std::string> groups = app->get_groups();

      // Print an Options badge if any options exist
      std::vector<const Option*> non_pos_options =
            app->get_options([](const Option* opt) { return opt->nonpositional(); });
      if(!non_pos_options.empty())
         out << " [" << get_label("OPTIONS") << "]";

      // Positionals need to be listed here
      std::vector<const Option*> positionals =
            app->get_options([](const Option* opt) { return opt->get_positional(); });

      // Print out positionals if any are left
      if(!positionals.empty()) {
         // Convert to help names
         std::vector<std::string> positional_names(positionals.size());
         std::transform(positionals.begin(), positionals.end(), positional_names.begin(), [this](const Option* opt) {
            return make_option_usage(opt);
         });

         out << " " << detail::join(positional_names, " ");
      }

      // Add a marker if subcommands are expected or optional
      if(!app->get_subcommands(
                   [](const CLI::App* subc) { return ((!subc->get_disabled()) && (!subc->get_name().empty())); })
                .empty()) {
         out << " " << (app->get_require_subcommand_min() == 0 ? "[" : "")
             << get_label(app->get_require_subcommand_max() < 2 || app->get_require_subcommand_min() > 1
                                ? "SUBCOMMAND"
                                : "SUBCOMMANDS")
             << (app->get_require_subcommand_min() == 0 ? "]" : "");
      }

      out << std::endl;

      return out.str();
   }

   /// This puts everything together
   std::string make_help(const App* app, std::string name, AppFormatMode mode) const {
      // This immediately forwards to the make_expanded method. This is done this way so that subcommands can
      // have overridden formatters
      if(mode == AppFormatMode::Sub || mode == AppFormatMode::SubCompact)
         return make_expanded(app, mode);

      std::stringstream out;
      if((app->get_name().empty()) && (app->get_parent() != nullptr)) {
         if(app->get_group() != "Subcommands") {
            out << app->get_group() << ':';
         }
      }

      out << make_description(app);
      out << make_usage(app, name);
      out << make_positionals(app);
      out << make_groups(app, mode);
      out << make_subcommands(app, mode);
      out << '\n'
          << make_footer(app);

      return out.str();
   }

   ///@}
   /// @name Options
   ///@{

   /// This prints out an option help line, either positional or optional form
   virtual std::string make_option(const Option* opt, bool is_positional) const {
      std::stringstream out;
      detail::format_help(
            out, make_option_name(opt, is_positional) + make_option_opts(opt), make_option_desc(opt), column_width_);
      return out.str();
   }

   /// @brief This is the name part of an option, Default: left column
   virtual std::string make_option_name(const Option* opt, bool is_positional) const {
      if(is_positional)
         return opt->get_name(true, false);

      return opt->get_name(false, true);
   }

   /// @brief This is the options part of the name, Default: combined into left column
   virtual std::string make_option_opts(const Option* opt) const {
      std::stringstream out;

      if(!opt->get_option_text().empty()) {
         out << " " << opt->get_option_text();
      } else {
         if(opt->get_type_size() != 0) {
            if(!opt->get_type_name().empty())
               out << " " << get_label(opt->get_type_name());
            if(!opt->get_default_str().empty())
               out << " [" << opt->get_default_str() << "] ";
            if(opt->get_expected_max() == detail::expected_max_vector_size)
               out << " ...";
            else if(opt->get_expected_min() > 1)
               out << " x " << opt->get_expected();

            if(opt->get_required())
               out << " " << get_label("REQUIRED");
         }
         if(!opt->get_envname().empty())
            out << " (" << get_label("Env") << ":" << opt->get_envname() << ")";
         if(!opt->get_needs().empty()) {
            out << " " << get_label("Needs") << ":";
            for(const Option* op: opt->get_needs())
               out << " " << op->get_name();
         }
         if(!opt->get_excludes().empty()) {
            out << " " << get_label("Excludes") << ":";
            for(const Option* op: opt->get_excludes())
               out << " " << op->get_name();
         }
      }
      return out.str();
   }

   /// @brief This is the description. Default: Right column, on new line if left column too large
   virtual std::string make_option_desc(const Option* opt) const { return opt->get_description(); }

   /// @brief This is used to print the name on the USAGE line
   virtual std::string make_option_usage(const Option* opt) const {
      // Note that these are positionals usages
      std::stringstream out;
      out << make_option_name(opt, true);
      if(opt->get_expected_max() >= detail::expected_max_vector_size)
         out << "...";
      else if(opt->get_expected_max() > 1)
         out << "(" << opt->get_expected() << "x)";

      return opt->get_required() ? out.str() : "[" + out.str() + "]";
   }

   ///@}
};

// [CLI11:formatter_hpp:end]
}// namespace CLI
